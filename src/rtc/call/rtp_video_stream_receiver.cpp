#include "rtc/call/rtp_video_stream_receiver.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <plog/Log.h>

namespace naivertc {

RtpVideoStreamReceiver::RtpVideoStreamReceiver(Configuration config, 
                                               std::shared_ptr<TaskQueue> task_queue) 
    : config_(std::move(config)), 
      task_queue_(std::move(task_queue)) {}

RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {}

void RtpVideoStreamReceiver::OnRtcpPacket(CopyOnWriteBuffer in_packet) {
    // task_queue_->Async([](){

    // });
}

void RtpVideoStreamReceiver::OnRtpPacket(RtpPacketReceived in_packet) {
    task_queue_->Async([this, in_packet=std::move(in_packet)](){
        this->HandleReceivedPacket(std::move(in_packet));
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

        this->HandleReceivedPacket(std::move(recovered_packet));
    });
}

// Private methods
void RtpVideoStreamReceiver::HandleReceivedPacket(const RtpPacketReceived& packet) {
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
