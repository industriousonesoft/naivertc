#include "rtc/rtp_rtcp/rtp_sender_video.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"
#include "rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer_h264.hpp"
#include <plog/Log.h>

namespace naivertc {

RtpSenderVideo::RtpSenderVideo(video::CodecType codec_type,
                               Clock* clock,
                               RtpSender* packet_sender) 
    : codec_type_(codec_type),
      clock_(clock),
      packet_sender_(packet_sender),
      current_playout_delay_{-1, -1},
      playout_delay_pending_(false) {
          
    if (codec_type_ == video::CodecType::H264) {
        rtp_packetizer_ = std::make_unique<RtpH264Packetizer>();
    }
}
    
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

    MaybeUpdateCurrentPlayoutDelay(video_header);
    // Key frame
    if (video_header.frame_type == video::FrameType::KEY) {
        // Force playout delay on key frame, if set.
        if (current_playout_delay_.IsAvailable()) {
            playout_delay_pending_ = true;
        }
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

    std::shared_ptr<RtpPacketToSend> single_packet = packet_sender_->AllocatePacket();
    if (packet_capacity > single_packet->capacity()) {
        PLOG_WARNING << "The maximum RTP packet capacity without FEC or RTX overhead should less than the capacity of allocated RTP packet.";
        return false;
    }

    single_packet->set_payload_type(payload_type);
    single_packet->set_timestamp(rtp_timestamp);
    single_packet->set_capture_time_ms(capture_time_ms);

    // TODO: To calculate absolute capture time and add to extension

    auto first_packet = std::make_shared<RtpPacketToSend>(*single_packet);
    auto middle_packet = std::make_shared<RtpPacketToSend>(*single_packet);
    auto last_packet = std::make_shared<RtpPacketToSend>(*single_packet);

    AddRtpHeaderExtensions(single_packet);
    assert(packet_capacity > single_packet->header_size());
    AddRtpHeaderExtensions(first_packet);
    assert(packet_capacity > first_packet->header_size());
    AddRtpHeaderExtensions(middle_packet);
    assert(packet_capacity > middle_packet->header_size());
    AddRtpHeaderExtensions(last_packet);
    assert(packet_capacity > last_packet->header_size());

    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = packet_capacity - middle_packet->header_size();
    limits.single_packet_reduction_size = single_packet->header_size() - middle_packet->header_size();
    limits.first_packet_reduction_size = first_packet->header_size() - middle_packet->header_size();
    limits.last_packet_reduction_size = last_packet->header_size() - middle_packet->header_size();

    if (codec_type_ == video::CodecType::H264) {
        dynamic_cast<RtpH264Packetizer*>(rtp_packetizer_.get())->Packetize(payload, limits, h264::PacketizationMode::NON_INTERLEAVED);
    } else {
        PLOG_WARNING << "Unsupported codec type.";
        return false;
    }

    const bool allow_retransmission = expected_retransmission_time_ms.has_value();

    const size_t num_of_packets = rtp_packetizer_->NumberOfPackets();

    if (num_of_packets == 0) {
        PLOG_VERBOSE << "No packets packetized.";
        return false;
    }

    size_t packetized_payload_size = 0;
    std::vector<std::shared_ptr<RtpPacketToSend>> rtp_packets;

    for (size_t i = 0; i < num_of_packets; ++i) {
        std::shared_ptr<RtpPacketToSend> packet;
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
            // There are more than one middle packet, so we need to create a new one instead of std::move
            packet = std::make_shared<RtpPacketToSend>(*middle_packet);
            expected_payload_capacity = limits.max_payload_size;
        }

        packet->set_is_first_packet_of_frame(i == 0);

        if (!rtp_packetizer_->NextPacket(packet.get())) {
            return false;
        }

        assert(packet->payload_size() <= expected_payload_capacity);

        packet->set_allow_retransmission(allow_retransmission);
        packet->set_is_key_frame(video_header.frame_type == video::FrameType::KEY);

        // TODO: Put packetization finish timestap into extension

        packet->set_fec_protection_need(fec_enabled);

        // FIXME: Do we really need to build a red packet here, like what the WebRTC does? 
        // and I think we just need to set the red flag.
        // NOTE: WebRTC中在此处新建伪RED包的作用似乎并不大，此处将为进行RED_FEC封装的包一律视为非RED包
        packet->set_is_red(false);
        packet->set_red_protection_need(packet_sender_->red_enabled());
        packet->set_packet_type(RtpPacketType::VIDEO);
        packetized_payload_size += packet->payload_size();
        rtp_packets.emplace_back(std::move(packet));

    } // end of for

    // AV1 and H264 packetizers may produce less packetized bytes than unpacketized.
    if (packetized_payload_size >= payload.size() /* unpacketized payload size */) {
        // TODO: Calculate packetization overhead bitrate.
    }

    if (!packet_sender_->EnqueuePackets(std::move(rtp_packets))) {
        PLOG_WARNING << "Failed to enqueue packets into packet sender.";
        return false;
    }

    // FIXME: H264 maybe reset always?
    if (video_header.frame_type == video::FrameType::KEY) {
        playout_delay_pending_ = false;
    }
    return true;
}

// Private methods
void RtpSenderVideo::AddRtpHeaderExtensions(std::shared_ptr<RtpPacketToSend> packet) {
    if (playout_delay_pending_) {
        packet->SetExtension<rtp::PlayoutDelayLimits>(current_playout_delay_.min_ms, current_playout_delay_.max_ms);
    }
    // TODO: Support more extensions
}

void RtpSenderVideo::MaybeUpdateCurrentPlayoutDelay(const RtpVideoHeader& header) {
    auto requested_delay = header.playout_delay;
    if (!requested_delay.IsAvailable()) {
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

    if (!playout_delay_pending_) {
        current_playout_delay_ = requested_delay;
        playout_delay_pending_ = true;
        return;
    }

    if ((requested_delay.min_ms == -1 ||
        requested_delay.min_ms == current_playout_delay_.min_ms) &&
       (requested_delay.max_ms == -1 ||
        requested_delay.max_ms == current_playout_delay_.max_ms)) {
        // No change, ignore.
        return;
    }

    if (requested_delay.min_ms == -1 && requested_delay.max_ms >= 0) {
        requested_delay.min_ms = std::min(current_playout_delay_.min_ms, requested_delay.max_ms);
    }
    if (requested_delay.max_ms == -1) {
        requested_delay.max_ms = std::max(current_playout_delay_.max_ms, requested_delay.min_ms);
    }

    current_playout_delay_ = requested_delay;
    playout_delay_pending_ = true;
}
    
} // namespace naivertc
