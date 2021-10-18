#include "rtc/call/rtp_video_stream_receiver.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <plog/Log.h>

namespace naivertc {

RtpVideoStreamReceiver::RtpVideoStreamReceiver(Configuration config,
                                               std::shared_ptr<Clock> clock,
                                               std::shared_ptr<TaskQueue> task_queue) 
    : config_(std::move(config)), 
      task_queue_(std::move(task_queue)),
      nack_module_(config.nack_enabled ? std::make_unique<NackModule>(clock, 
                                                                      kDefaultSendNackDelayMs, 
                                                                      kDefaultUpdateInterval, 
                                                                      task_queue_) 
                                       : nullptr) {}

RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {}

void RtpVideoStreamReceiver::OnRtcpPacket(CopyOnWriteBuffer in_packet) {
    // task_queue_->Async([](){

    // });
}

void RtpVideoStreamReceiver::OnRtpPacket(RtpPacketReceived in_packet) {
    task_queue_->Async([this, in_packet=std::move(in_packet)](){
        this->OnReceivedPacket(std::move(in_packet));
    });
}

// TODO: Using RtpPacketReceived as parameters for thread-safety
void RtpVideoStreamReceiver::OnRecoveredPacket(const uint8_t* packet, size_t packet_size) {
    task_queue_->Async([this, packet, packet_size](){
        RtpPacketReceived recovered_packet;
        if (!recovered_packet.Parse(packet, packet_size)) {
            PLOG_WARNING << "Failed to parse recovered packet as RTP packet.";
            return;
        }
        if (recovered_packet.payload_type() == config_.red_payload_type) {
            PLOG_WARNING << "Discarding recovered packet with RED encapsulation.";
            return;
        }

        // TODO: Identify header extensions of RTP packet.
        recovered_packet.set_payload_type_frequency(kVideoPayloadTypeFrequency);

        this->OnReceivedPacket(std::move(recovered_packet));
    });
}

// Private methods
void RtpVideoStreamReceiver::OnReceivedPacket(const RtpPacketReceived& packet) {
    // Padding or keep-alive packet
    if (packet.payload_size() == 0) {
        HandleEmptyPacket(packet.sequence_number());
        return;
    }
    if (packet.payload_type() == config_.red_payload_type) {
        HandleRedPacket(packet);
        return;
    }
    const auto type_it = payload_type_map_.find(packet.payload_type());
    if (type_it == payload_type_map_.end()) {
        PLOG_WARNING << "No RTP depacketizer found for payload type=" 
                     << packet.payload_type();
        return;
    }
    std::optional<RtpDepacketizer::DepacketizedPayload> depacketized_payload = type_it->second->Depacketize(packet.PayloadBuffer());
    if (!depacketized_payload) {
        PLOG_WARNING << "Failed to depacketize RTP payload.";
        return;
    }

    OnDepacketizedPayload(std::move(*depacketized_payload), packet);
}

void RtpVideoStreamReceiver::OnDepacketizedPayload(RtpDepacketizer::DepacketizedPayload depacketized_payload, const RtpPacketReceived& rtp_packet) {
    auto assembling_packet = std::make_unique<RtpVideoFrameAssembler::Packet>(depacketized_payload.video_header,
                                                                              depacketized_payload.video_codec_header,
                                                                              rtp_packet.sequence_number(),
                                                                              rtp_packet.timestamp());
    RtpVideoHeader& video_header = assembling_packet->video_header;
    video_header.is_last_packet_in_frame |= rtp_packet.marker();

    // TODO: Collect packet info

    if (auto extension = rtp_packet.GetExtension<rtp::PlayoutDelayLimits>()) {
        video_header.playout_delay.min_ms = extension->min_ms();
        video_header.playout_delay.max_ms = extension->max_ms();
    }

    // TODO: Support more RTP header extensions

    if (!rtp_packet.is_recovered()) {
        // TODO: Update packet receive timestamp
    }

    if (nack_module_) {
        const bool is_keyframe = video_header.is_first_packet_in_frame && video_header.frame_type == video::FrameType::KEY;
        // TODO: Pack int packet info
        size_t nacks_sent = nack_module_->InsertPacket(rtp_packet.sequence_number(), is_keyframe, rtp_packet.is_recovered());
    }

    if (depacketized_payload.video_payload.empty()) {
        HandleEmptyPacket(rtp_packet.sequence_number());
        // TODO: Send buffered RTCP feedback.
        return;
    }

    // H264
    if (video_header.codec_type == video::CodecType::H264) {

    }else {

    }


}

void RtpVideoStreamReceiver::HandleEmptyPacket(uint16_t seq_num) {
    
}

void RtpVideoStreamReceiver::HandleRedPacket(const RtpPacketReceived& packet) {
    if (packet.payload_type() == config_.red_payload_type && 
        packet.payload_size() > 0) {
        if (packet.payload()[0] == config_.ulpfec_payload_type) {
            // Handle packet recovered by FEC as a empty packet to 
            // avoid NACKing it.
            HandleEmptyPacket(packet.sequence_number());
        }
    }

    // TODO: Handle RED packet using UlpFEC receiver.
}

} // namespace naivertc
