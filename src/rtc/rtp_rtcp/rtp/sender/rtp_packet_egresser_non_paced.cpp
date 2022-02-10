#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.hpp"

namespace naivertc {

RtpPacketEgresser::NonPacedPacketSender::NonPacedPacketSender(RtpPacketEgresser* const sender, 
                                                              SequenceNumberAssigner* seq_num_assigner) 
        : transport_sequence_number_(0),
          sender_(sender),
          seq_num_assigner_(seq_num_assigner) {}

RtpPacketEgresser::NonPacedPacketSender::~NonPacedPacketSender() = default;

void RtpPacketEgresser::NonPacedPacketSender::EnqueuePackets(std::vector<RtpPacketToSend> packets) {
    for (auto& packet : packets) {
        PrepareForSend(packet);
        sender_->SendPacket(packet);
    }
    auto fec_packets = sender_->FetchFecPackets();
    if (!fec_packets.empty()) {
        // Don't generate sequence numbers for flexfec, they are already running on
        // an internally maintained sequence.
        // flexfec_ssrc有值表示使用的是FlexFEX，否则是UlpFEC，
        // FEC包有两种传输方式：
        // 1）另开一路流(ssrc区分)传输
        // 2）使用RED封装作为冗余编码传输
        // webRTC中的实现FlexFEX有独立的SSRC(意味着sequence number也是独立的)
        // 而UlpFEX则是和原媒体流共用SSRC，因此需要给生成的fec包设置新的sequence number
        const bool fec_red_enabled = !sender_->flex_fec_ssrc();
        for (auto& packet : fec_packets) {
            if (fec_red_enabled) {
                seq_num_assigner_->AssignSequenceNumber(packet);
            }
            PrepareForSend(packet);
        }
        PLOG_VERBOSE_IF(true) << "Enqueued " << fec_packets.size() << " FEC packets after sending media packets.";
        EnqueuePackets(std::move(fec_packets));
    }
}

// Private methods
void RtpPacketEgresser::NonPacedPacketSender::PrepareForSend(RtpPacketToSend& packet) {
    if (!packet.SetExtension<rtp::TransportSequenceNumber>(++transport_sequence_number_)) {
        --transport_sequence_number_;
    }
    // FIXME: The calls below will be failed since the payload size of packet has set.
    packet.ReserveExtension<rtp::TransmissionTimeOffset>();
    packet.ReserveExtension<rtp::AbsoluteSendTime>();
}
    
} // namespace naivertc
