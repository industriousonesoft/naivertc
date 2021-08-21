#include "rtc/rtp_rtcp/rtp/rtp_non_paced_packet_sender.hpp"

namespace naivertc {

RtpNonPacedPacketSender::RtpNonPacedPacketSender(std::shared_ptr<RtpSenderEgress> sender, 
                                                 std::shared_ptr<RtpPacketSequencer> packet_sequencer) 
        : transport_sequence_number_(0),
          sender_(sender),
          packet_sequencer_(packet_sequencer) {}

RtpNonPacedPacketSender::~RtpNonPacedPacketSender() = default;

void RtpNonPacedPacketSender::EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) {
    std::vector<std::shared_ptr<RtpPacketToSend>> cumulative_fec_packets;
    for (auto& packet : packets) {
        PrepareForSend(packet);
        sender_->SendPacket(packet, [&cumulative_fec_packets](std::vector<std::shared_ptr<RtpPacketToSend>> fec_packets){
            for (auto packet : fec_packets) {
                cumulative_fec_packets.push_back(std::move(packet));
            }
        });
    }

    if (!cumulative_fec_packets.empty()) {
        // Don't generate sequence numbers for flexfec, they are already running on
        // an internally maintained sequence.
        // TODO: 在哪设置flexfec_ssrc？ 为什么有flexfec_ssrc就代表有内部的序号？
        const bool need_generate_sequence_numbers = sender_->flexfec_ssrc().has_value() == false;
        for (auto& packet : cumulative_fec_packets) {
            if (need_generate_sequence_numbers) {
                packet_sequencer_->Sequence(packet);
            }
            PrepareForSend(packet);
        }
        EnqueuePackets(std::move(cumulative_fec_packets));
    }
}

// Private methods
void RtpNonPacedPacketSender::PrepareForSend(std::shared_ptr<RtpPacketToSend> packet) {
    if (!packet->SetExtension<rtp::TransportSequenceNumber>(++transport_sequence_number_)) {
        --transport_sequence_number_;
    }
    // TODO: Do we need to reserver extension here??
    packet->ReserveExtension<rtp::TransmissionTimeOffset>();
    packet->ReserveExtension<rtp::AbsoluteSendTime>();
}
    
} // namespace naivertc
