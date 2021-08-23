#include "rtc/rtp_rtcp/rtp/rtp_packet_pacer_non_paced.hpp"

namespace naivertc {

RtpNonPacedPacketPacer::RtpNonPacedPacketPacer(std::shared_ptr<RtpPacketSender> sender, 
                                                 std::shared_ptr<RtpPacketSequencer> packet_sequencer) 
        : transport_sequence_number_(0),
          sender_(sender),
          packet_sequencer_(packet_sequencer) {}

RtpNonPacedPacketPacer::~RtpNonPacedPacketPacer() = default;

void RtpNonPacedPacketPacer::EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) {
    for (auto& packet : packets) {
        PrepareForSend(packet);
        sender_->SendPacket(packet);
    }
    auto fec_packets = sender_->FetchFecPackets();
    if (!fec_packets.empty()) {
        // Don't generate sequence numbers for flexfec, they are already running on
        // an internally maintained sequence.
        // TODO: 在哪设置flexfec_ssrc？ 为什么有flexfec_ssrc就代表有内部的序号？
        // TODO: packet_sequencer_能不能已到sender里面去？？为什么Pacing模块中并没有类似逻辑
        // const bool need_generate_sequence_numbers = sender_->flexfec_ssrc().has_value() == false;
        for (auto& packet : fec_packets) {
            // TODO: To set sequence number for FEC packet
            // if (need_generate_sequence_numbers) {
            //     packet_sequencer_->Sequence(packet);
            // }
            PrepareForSend(packet);
        }
        EnqueuePackets(std::move(fec_packets));
    }
}

// Private methods
void RtpNonPacedPacketPacer::PrepareForSend(std::shared_ptr<RtpPacketToSend> packet) {
    if (!packet->SetExtension<rtp::TransportSequenceNumber>(++transport_sequence_number_)) {
        --transport_sequence_number_;
    }
    // TODO: Do we need to reserver extension here??
    packet->ReserveExtension<rtp::TransmissionTimeOffset>();
    packet->ReserveExtension<rtp::AbsoluteSendTime>();
}
    
} // namespace naivertc
