#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.hpp"

namespace naivertc {

RtpPacketEgresser::NonPacedPacketSender::NonPacedPacketSender(RtpPacketEgresser* const sender) 
        : sender_(sender) {}

RtpPacketEgresser::NonPacedPacketSender::~NonPacedPacketSender() = default;

void RtpPacketEgresser::NonPacedPacketSender::EnqueuePackets(std::vector<RtpPacketToSend> packets) {
    for (auto& packet : packets) {
        sender_->SendPacket(packet);
    }
    auto fec_packets = sender_->FetchFecPackets();
    if (!fec_packets.empty()) {
        PLOG_VERBOSE_IF(true) << "Enqueued " << fec_packets.size() << " FEC packets after sending media packets.";
        EnqueuePackets(std::move(fec_packets));
    }
}
    
} // namespace naivertc
