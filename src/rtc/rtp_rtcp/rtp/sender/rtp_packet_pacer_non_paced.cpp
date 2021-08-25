#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_pacer_non_paced.hpp"

namespace naivertc {

RtpNonPacedPacketPacer::RtpNonPacedPacketPacer(std::shared_ptr<RtpPacketSenderEgress> sender, 
                                               std::shared_ptr<SequenceNumberAssigner> packet_sequencer,
                                               std::shared_ptr<TaskQueue> task_queue) 
        : transport_sequence_number_(0),
          sender_(sender),
          packet_sequencer_(packet_sequencer),
          task_queue_(task_queue) {}

RtpNonPacedPacketPacer::~RtpNonPacedPacketPacer() = default;

void RtpNonPacedPacketPacer::EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) {
    task_queue_->Async([this, packets=std::move(packets)](){
        for (auto& packet : packets) {
            PrepareForSend(packet);
            sender_->SendPacket(packet);
        }
        auto fec_packets = sender_->FetchFecPackets();
        if (!fec_packets.empty()) {
            // Don't generate sequence numbers for flexfec, they are already running on
            // an internally maintained sequence.
            // flexfec_ssrc有值表示使用的是FlexFEX，否则是UlpFEC，
            // FEC包有两种传输方式：1）另开一路流(ssrc区分)传输，2）使用RED封装作为冗余编码传输
            // webRTC中的实现FlexFEX有独立的SSRC(意味着sequence number也是独立的)
            // 而UlpFEX则是和原媒体流共用SSRC，因此需要给生成的fec包设置新的sequence number
            // TODO: packet_sequencer_能不能已到sender里面去？？
            // const bool ulpfec_enabled = sender_->flexfec_ssrc().has_value() == false;
            for (auto& packet : fec_packets) {
                // TODO: To set sequence number for UlpFEC packet
                // if (ulpfec_enabled) {
                //     packet_sequencer_->AssignSequenceNumber(packet);
                // }
                PrepareForSend(packet);
            }
            EnqueuePackets(std::move(fec_packets));
        }
    });
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
