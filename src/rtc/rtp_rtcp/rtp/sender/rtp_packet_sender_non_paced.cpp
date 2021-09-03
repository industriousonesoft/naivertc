#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sender.hpp"

namespace naivertc {

RtpPacketSender::NonPacedPacketSender::NonPacedPacketSender(RtpPacketSender* const rtp_sender) 
        : transport_sequence_number_(0),
          rtp_sender_(rtp_sender) {}

RtpPacketSender::NonPacedPacketSender::~NonPacedPacketSender() = default;

void RtpPacketSender::NonPacedPacketSender::EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) {
    for (auto& packet : packets) {
        PrepareForSend(packet);
        rtp_sender_->packet_sender_.SendPacket(packet);
    }
    auto fec_packets = rtp_sender_->packet_sender_.FetchFecPackets();
    if (!fec_packets.empty()) {
        // Don't generate sequence numbers for flexfec, they are already running on
        // an internally maintained sequence.
        // flexfec_ssrc有值表示使用的是FlexFEX，否则是UlpFEC，
        // FEC包有两种传输方式：1）另开一路流(ssrc区分)传输，2）使用RED封装作为冗余编码传输
        // webRTC中的实现FlexFEX有独立的SSRC(意味着sequence number也是独立的)
        // 而UlpFEX则是和原媒体流共用SSRC，因此需要给生成的fec包设置新的sequence number
        
        const bool fec_red_enabled = rtp_sender_->fec_generator_->fec_ssrc().has_value() == false;
        for (auto& packet : fec_packets) {
            if (fec_red_enabled) {
                rtp_sender_->packet_sequencer_.AssignSequenceNumber(packet);
            }
            PrepareForSend(packet);
        }
        EnqueuePackets(std::move(fec_packets));
    }
}

// Private methods
void RtpPacketSender::NonPacedPacketSender::PrepareForSend(std::shared_ptr<RtpPacketToSend> packet) {
    if (!packet->SetExtension<rtp::TransportSequenceNumber>(++transport_sequence_number_)) {
        --transport_sequence_number_;
    }
    // TODO: Why do we need to reserver extension here??
    packet->ReserveExtension<rtp::TransmissionTimeOffset>();
    packet->ReserveExtension<rtp::AbsoluteSendTime>();
}
    
} // namespace naivertc
