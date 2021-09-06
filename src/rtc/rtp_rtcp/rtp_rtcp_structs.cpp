#include "rtc/rtp_rtcp/rtp_rtcp_structs.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"

namespace naivertc {

// RtpState
RtpState::RtpState()
    : sequence_num(0),
      start_timestamp(0),
      timestamp(0),
      capture_time_ms(-1),
      last_timestamp_time_ms(-1),
      ssrc_has_acked(false) {}

RtpState::~RtpState() = default;

// RtpPacketCounter
RtpPacketCounter::RtpPacketCounter()
    : header_bytes(0), 
      payload_bytes(0), 
      padding_bytes(0), 
      packets(0) {}

RtpPacketCounter::RtpPacketCounter(const RtpPacket& packet) 
    : header_bytes(packet.header_size()), 
      payload_bytes(packet.payload_size()), 
      padding_bytes(packet.padding_size()), 
      packets(1) {}
    
RtpPacketCounter::~RtpPacketCounter() = default;

bool RtpPacketCounter::operator==(const RtpPacketCounter& other) const {
    return header_bytes == other.header_bytes &&
            payload_bytes == other.payload_bytes &&
            padding_bytes == other.padding_bytes && 
            packets == other.packets;
}

RtpPacketCounter& RtpPacketCounter::operator+=(const RtpPacketCounter& other) {
    this->header_bytes += other.header_bytes;
    this->payload_bytes += other.payload_bytes;
    this->padding_bytes += other.padding_bytes;
    this->packets += other.packets;
    return *this;
}

size_t RtpPacketCounter::TotalBytes() const {
    return header_bytes + payload_bytes + padding_bytes;
}

// RtpSentCounters
RtpSentCounters::RtpSentCounters() = default;
RtpSentCounters::~RtpSentCounters() = default;

RtpSentCounters& RtpSentCounters::operator+=(const RtpSentCounters& other) {
    this->transmitted += other.transmitted;
    this->retransmitted += other.retransmitted;
    this->fec += other.fec;
    return *this;
}

} // namespace naivertc