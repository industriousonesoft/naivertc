#include "rtc/rtp_rtcp/rtp_sender_video.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"
#include "rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer_h264.hpp"
#include "rtc/rtp_rtcp/rtp_sender.hpp"
#include <plog/Log.h>

namespace naivertc {

RtpSenderVideo::RtpSenderVideo(Clock* clock, RtpSender* packet_sender) 
    : clock_(clock),
      packet_sender_(packet_sender),
      current_playout_delay_{-1, -1},
      playout_delay_pending_(false) {}
    
RtpSenderVideo::~RtpSenderVideo() {}

bool RtpSenderVideo::Send(int payload_type,
                          uint32_t rtp_timestamp, 
                          int64_t capture_time_ms,
                          RtpVideoHeader video_header,
                          ArrayView<const uint8_t> payload,
                          std::optional<int64_t> expected_retransmission_time_ms,
                          std::optional<int64_t> estimated_capture_clock_offset_ms) {
    RTC_RUN_ON(&sequence_checker_);
    
    if (payload.empty()) {
        return false;
    }

    // TODO: No FEC protection for upper temporal layers, if used
    const bool fec_enabled = packet_sender_->fec_enabled();

    // Calculate maximum size of packet including RTP packets.
    size_t packet_capacity = packet_sender_->max_rtp_packet_size();
    // Extra space left in case packet will be reset using FEC or RTX.
    // Reserve overhead size of FEC packet
    if (fec_enabled) {
        packet_capacity -= packet_sender_->FecPacketOverhead();
    }
    // Reserve overhead size of RTX packet
    if (packet_sender_->rtx_mode() != RtxMode::OFF) {
        packet_capacity -= kRtxHeaderSize;
    }

    RtpPacketToSend single_packet = packet_sender_->GeneratePacket();
    if (packet_capacity > single_packet.capacity()) {
        PLOG_WARNING << "The maximum RTP packet capacity without FEC or RTX overhead should less than the capacity of allocated RTP packet.";
        return false;
    }

    const bool allow_retransmission = expected_retransmission_time_ms.has_value();

    single_packet.set_payload_type(payload_type);
    single_packet.set_timestamp(rtp_timestamp);
    single_packet.set_capture_time_ms(capture_time_ms);
    single_packet.set_is_key_frame(video_header.frame_type == video::FrameType::KEY);
    single_packet.set_allow_retransmission(allow_retransmission);
    single_packet.set_fec_protection_need(fec_enabled);

    // TODO: To calculate absolute capture time and add to extension

    // Force playout delay on key frame, if set.
    UpdateCurrentPlayoutDelay(video_header.playout_delay);
    if (video_header.frame_type == video::FrameType::KEY) {
        if (current_playout_delay_.IsValid()) {
            playout_delay_pending_ = true;
        }
    }
    // Set palyout delay extension
    if (playout_delay_pending_) {
        single_packet.SetExtension<rtp::PlayoutDelayLimits>(current_playout_delay_);
    }

    RtpPacketToSend first_packet = single_packet;
    RtpPacketToSend middle_packet = single_packet;
    RtpPacketToSend last_packet = single_packet;

    AddRtpHeaderExtensions(/*first_packet=*/true, /*last_packet=*/true, single_packet);
    assert(packet_capacity > single_packet.header_size());
    AddRtpHeaderExtensions(/*first_packet=*/true, /*last_packet=*/false, first_packet);
    assert(packet_capacity > first_packet.header_size());
    AddRtpHeaderExtensions(/*first_packet=*/false, /*last_packet=*/false, middle_packet);
    assert(packet_capacity > middle_packet.header_size());
    AddRtpHeaderExtensions(/*first_packet=*/false, /*last_packet=*/true, last_packet);
    assert(packet_capacity > last_packet.header_size());

    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = packet_capacity - middle_packet.header_size();
    limits.single_packet_reduction_size = single_packet.header_size() - middle_packet.header_size();
    limits.first_packet_reduction_size = first_packet.header_size() - middle_packet.header_size();
    limits.last_packet_reduction_size = last_packet.header_size() - middle_packet.header_size();

    auto packetizer = Packetize(video_header.codec_type, payload, limits);

    if (packetizer == nullptr) {
        return false; 
    }

    const size_t num_of_packets = packetizer->NumberOfPackets();

    if (num_of_packets == 0) {
        PLOG_VERBOSE << "No packets packetized.";
        return false;
    }

    size_t packetized_payload_size = 0;
    std::vector<RtpPacketToSend> rtp_packets;

    for (size_t i = 0; i < num_of_packets; ++i) {
        std::optional<RtpPacketToSend> packet = std::nullopt;
        int expected_payload_capacity;
        if (num_of_packets == 1) {
            packet = std::move(single_packet);
            expected_payload_capacity = limits.max_payload_size - limits.single_packet_reduction_size;
        } else if (i == 0) {
            packet = std::move(first_packet);
            expected_payload_capacity = limits.max_payload_size - limits.first_packet_reduction_size;
        } else if (i == num_of_packets - 1) {
            packet = std::move(last_packet);
            expected_payload_capacity = limits.max_payload_size - limits.last_packet_reduction_size;
        } else {
            // Multiple packets in middle size will be created.
            packet = middle_packet;
            expected_payload_capacity = limits.max_payload_size;
        }
        assert(packet != std::nullopt);

        packet->set_is_first_packet_of_frame(i == 0);

        if (!packetizer->NextPacket(&packet.value())) {
            assert(false);
            return false;
        }

        assert(packet->payload_size() <= expected_payload_capacity);

        // TODO: Put packetization finish timestamp into extension

        // FIXME: Do we really need to build a red packet here, like what the WebRTC does? 
        // and I think we just need to set the red flag.
        // NOTE: WebRTC中在此处新建伪RED包的作用似乎并不大，此处将未使用RED_FEC封装的包一律视为非RED包
        packet->set_is_red(false);
        packet->set_red_protection_need(packet_sender_->red_enabled());
        packet->set_packet_type(RtpPacketType::VIDEO);
        packetized_payload_size += packet->payload_size();
        rtp_packets.emplace_back(std::move(*packet));

    } // end of for

    // Assign sequence number.
    if (!packet_sender_->AssignSequenceNumbers(rtp_packets)) {
        PLOG_WARNING << "Failed to assign sequence number to RTP packet before sending.";
        return false;
    }

    // Packtization overhead.
    CalcPacketizationOverhead(rtp_packets, /*unpacketized_payload_size=*/payload.size());
    
    // Send to network.
    if (!packet_sender_->EnqueuePackets(std::move(rtp_packets))) {
        PLOG_WARNING << "Failed to enqueue packets into packet sender.";
        return false;
    }

    // TODO: Check if the base layer of VP8 frame, which is the same as the key frame of H264.
    if (video_header.frame_type == video::FrameType::KEY) {
        // This frame will likely be delivered, no need to populate playout
        // delay extensions until it changes again.
        playout_delay_pending_ = false;
    }

    return true;
}

DataRate RtpSenderVideo::PacktizationOverheadBitrate() {
    RTC_RUN_ON(&sequence_checker_);
    return packetization_overhead_bitrate_stats_.Rate(clock_->now_ms()).value_or(DataRate::Zero());
}

// Private methods
void RtpSenderVideo::AddRtpHeaderExtensions(bool first_packet, 
                                            bool last_packet, 
                                            RtpPacketToSend& packet) {
    // TODO: Support more extensions
}

RtpPacketizer* RtpSenderVideo::Packetize(video::CodecType codec_type, 
                                         ArrayView<const uint8_t> payload, 
                                         const RtpPacketizer::PayloadSizeLimits& limits) {
    if (codec_type == video::CodecType::H264) {
        auto& packetizer = rtp_packetizers_[codec_type];
        if (packetizer == nullptr) {
            packetizer.reset(new RtpH264Packetizer());
        }
        dynamic_cast<RtpH264Packetizer*>(packetizer.get())->Packetize(payload, limits, h264::PacketizationMode::NON_INTERLEAVED);
        return packetizer.get();
    } else {
        PLOG_WARNING << "Unsupported codec type: " << codec_type;
        return nullptr;
    }
}

void RtpSenderVideo::UpdateCurrentPlayoutDelay(const video::PlayoutDelay& requested_delay) {
    if (!requested_delay.IsValid()) {
        return;
    }

    if (requested_delay.min_ms > rtp::PlayoutDelayLimits::kMaxMs ||
        requested_delay.max_ms > rtp::PlayoutDelayLimits::kMaxMs) {
        PLOG_WARNING << "Requested palyout delay value out of range, ignored.";
        return;
    }

    if (requested_delay.max_ms != -1 &&
        requested_delay.min_ms > requested_delay.max_ms) {
        PLOG_WARNING << "Requested playout delay values out of order";
        return;
    }

    if (!current_playout_delay_.IsValid()) {
        current_playout_delay_ = requested_delay;
        playout_delay_pending_ = true;
        return;
    }

    if (requested_delay == current_playout_delay_) {
        // No change, ignore.
        return;
    }

    if (requested_delay.min_ms == -1 && requested_delay.max_ms >= 0) {
        current_playout_delay_.min_ms = std::min(current_playout_delay_.min_ms, requested_delay.max_ms);
    }
    if (requested_delay.max_ms == -1) {
        current_playout_delay_.max_ms = std::max(current_playout_delay_.max_ms, requested_delay.min_ms);
    }

    playout_delay_pending_ = true;
}

void RtpSenderVideo::CalcPacketizationOverhead(ArrayView<const RtpPacketToSend> packets, 
                                               size_t unpacketized_payload_size) {
    size_t packetized_payload_size = 0;
    for (auto& packet : packets) {
        if (packet.packet_type() == RtpPacketType::VIDEO) {
            packetized_payload_size += packet.size();
        }
    }
    // AV1 and H264 packetizers may produce less packetized bytes than unpacketized.
    if (packetized_payload_size >= unpacketized_payload_size) {
        packetization_overhead_bitrate_stats_.Update(packetized_payload_size - unpacketized_payload_size, clock_->now_ms());
    }
}
    
} // namespace naivertc
