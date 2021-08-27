#include "rtc/rtp_rtcp/rtp/video/rtp_video_sender.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"
#include "rtc/rtp_rtcp/rtp/packetizer/rtp_h264_packetizer.hpp"
#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr size_t kRedForFecHeaderLength = 1;
    
} // namespac 

RtpVideoSender::RtpVideoSender(const Configuration& config, 
                               std::shared_ptr<RtpPacketSender> packet_sender, 
                               std::shared_ptr<TaskQueue> task_queue) 
    : clock_(config.clock),
      codec_type_(config.codec_type),
      red_payload_type_(config.red_payload_type),
      fec_overhead_bytes_(config.fec_overhead_bytes),
      fec_type_(config.fec_type),
      packet_sender_(packet_sender),
      task_queue_(task_queue),
      current_playout_delay_{-1, -1},
      playout_delay_pending_(false) {

    if (codec_type_ == video::CodecType::H264) {
        rtp_packetizer_ = std::make_shared<RtpH264Packetizer>();
    }
}
    
RtpVideoSender::~RtpVideoSender() {

}

bool RtpVideoSender::SendVideo(int payload_type,
                               uint32_t rtp_timestamp, 
                               int64_t capture_time_ms, 
                               ArrayView<const uint8_t> payload,
                               RtpVideoHeader video_header,
                               std::optional<int64_t> expected_retransmission_time_ms,
                               std::optional<int64_t> estimated_capture_clock_offset_ms) {
    return task_queue_->Sync<bool>([=, video_header=std::move(video_header)](){
        if (payload.empty()) {
            return false;
        }

        this->MaybeUpdateCurrentPlayoutDelay(video_header);
        // Key frame
        if (video_header.frame_type == video::FrameType::KEY) {
            // Force playout delay on key frame, if set.
            if (this->current_playout_delay_.IsAvailable()) {
                this->playout_delay_pending_ = true;
            }
        }

        // TODO: No FEC protection for upper temporal layers, if used
        const bool use_fec = fec_type_.has_value();

        // Calculate maximum size of packet including RTP packets.
        size_t packet_capacity = this->packet_sender_->max_rtp_packet_size();
        // Extra space left in case packet will be reset using FEC or RTX.
        // Reserve overhead size of FEC packet
        if (use_fec) {
            packet_capacity -= this->FecPacketOverhead();
        }
        // Reserve overhead size of RTX packet
        if (this->packet_sender_->rtx_mode() != RtxMode::OFF) {
            packet_capacity -= kRtxHeaderSize;
        }

        std::shared_ptr<RtpPacketToSend> single_packet = this->packet_sender_->AllocatePacket();
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
            std::static_pointer_cast<RtpH264Packetizer>(this->rtp_packetizer_)->Packetize(payload, limits, h264::PacketizationMode::NON_INTERLEAVED);
        }else {
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
            }else if (i == 0) {
                packet = std::move(first_packet);
                expected_payload_capacity = limits.max_payload_size - limits.first_packet_reduction_size;
            }else if (i == num_of_packets - 1) {
                packet = std::move(last_packet);
                expected_payload_capacity = limits.max_payload_size - limits.last_packet_reduction_size;
            }else {
                // There are more than one middle packet, so we need to create a new one instead of std::move
                packet = std::make_shared<RtpPacketToSend>(*middle_packet);
                expected_payload_capacity = limits.max_payload_size;
            }

            packet->set_is_first_packet_of_frame(i == 0);

            if (!this->rtp_packetizer_->NextPacket(packet.get())) {
                return false;
            }

            assert(packet->payload_size() <= expected_payload_capacity);

            packet->set_allow_retransmission(allow_retransmission);
            packet->set_is_key_frame(video_header.frame_type == video::FrameType::KEY);

            // TODO: Put packetization finish timestap into extension

            packet->set_fec_protected_packet(use_fec);

            // FIXME: Do we really need to build a red packet here, like what the WebRTC does? 
            // and I think we just need to set the red flag.
            packet->set_is_red(this->red_payload_type_.has_value());
            packet->set_packet_type(RtpPacketType::VIDEO);
            packetized_payload_size += packet->payload_size();
            rtp_packets.emplace_back(std::move(packet));

        } // end of for

        // AV1 and H264 packetizers may produce less packetized bytes than unpacketized.
        if (packetized_payload_size >= payload.size() /* unpacketized payload size */) {
            // TODO: Calculate packetization overhead bitrate.
        }

        if (!this->packet_sender_->EnqueuePackets(std::move(rtp_packets))) {
            PLOG_WARNING << "Failed to enqueue packets into packet sender.";
            return false;
        }

        // FIXME: H264 maybe reset always?
        if (video_header.frame_type == video::FrameType::KEY) {
            this->playout_delay_pending_ = false;
        }
        return true;
    });
}

// Private methods
void RtpVideoSender::AddRtpHeaderExtensions(std::shared_ptr<RtpPacketToSend> packet) {
    if (playout_delay_pending_) {
        packet->SetExtension<rtp::PlayoutDelayLimits>(current_playout_delay_.min_ms, current_playout_delay_.max_ms);
    }
    // TODO: Support more extensions
}

void RtpVideoSender::MaybeUpdateCurrentPlayoutDelay(const RtpVideoHeader& header) {
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

size_t RtpVideoSender::FecPacketOverhead() const {
    size_t overhead = fec_overhead_bytes_;
    if (red_payload_type_.has_value()) {
        // RED packet overhead 
        overhead += kRedForFecHeaderLength;
        // ULP FEC
        if (fec_type_ == FecGenerator::FecType::ULP_FEC) {
            // For ULPFEC, the overhead is the FEC headers plus RED for FEC header 
            // plus anthing int RTP packet beyond the 12 bytes base header, e.g.:
            // CSRC list, extensions...
            // This reason for the header extensions to be included here is that
            // from an FEC viewpoint, they are part of the payload to be protected.
            // and the base RTP header is already protected by the FEC header.
            overhead += packet_sender_->FecOrPaddingPacketMaxRtpHeaderLength() - kRtpHeaderSize;
        }
    }
    return overhead;
}
    
} // namespace naivertc
