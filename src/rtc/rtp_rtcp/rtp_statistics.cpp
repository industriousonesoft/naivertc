#include "rtc/rtp_rtcp/rtp_statistics.hpp"
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
      num_packets(0) {}

RtpPacketCounter::RtpPacketCounter(const RtpPacket& packet) 
    : header_bytes(packet.header_size()), 
      payload_bytes(packet.payload_size()), 
      padding_bytes(packet.padding_size()), 
      num_packets(1) {}
    
RtpPacketCounter::~RtpPacketCounter() = default;

bool RtpPacketCounter::operator==(const RtpPacketCounter& other) const {
    return header_bytes == other.header_bytes &&
           payload_bytes == other.payload_bytes &&
           padding_bytes == other.padding_bytes && 
           num_packets == other.num_packets;
}

RtpPacketCounter& RtpPacketCounter::operator+=(const RtpPacketCounter& other) {
    this->header_bytes += other.header_bytes;
    this->payload_bytes += other.payload_bytes;
    this->padding_bytes += other.padding_bytes;
    this->num_packets += other.num_packets;
    return *this;
}

RtpPacketCounter& RtpPacketCounter::operator-=(const RtpPacketCounter& other) {
    assert(this->header_bytes >= other.header_bytes);
    this->header_bytes -= other.header_bytes;
    assert(this->payload_bytes >= other.payload_bytes);
    this->payload_bytes -= other.payload_bytes;
    assert(this->padding_bytes >= other.padding_bytes);
    this->padding_bytes -= other.padding_bytes;
    assert(this->num_packets >= other.num_packets);
    this->num_packets -= other.num_packets;
    return *this;
}

void RtpPacketCounter::AddPacket(const RtpPacket& packet) {
    ++num_packets;
    header_bytes += packet.header_size();
    padding_bytes += packet.padding_size();
    payload_bytes += packet.payload_size();
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

// RtpStreamDataCounters
RtpStreamDataCounters::RtpStreamDataCounters() = default;
RtpStreamDataCounters::~RtpStreamDataCounters() = default;

RtpStreamDataCounters& RtpStreamDataCounters::operator+=(const RtpStreamDataCounters& other) {
    this->transmitted += other.transmitted;
    this->retransmitted += other.retransmitted;
    this->fec += other.fec;
    if (other.first_packet_time_ms != -1 &&
        (other.first_packet_time_ms < this->first_packet_time_ms || this->first_packet_time_ms == -1)) {
        // Prefer the oldest time.
        this->first_packet_time_ms = other.first_packet_time_ms;
    }
    return *this;
}
    
RtpStreamDataCounters& RtpStreamDataCounters::operator-=(const RtpStreamDataCounters& other) {
    this->transmitted -= other.transmitted;
    this->retransmitted -= other.retransmitted;
    this->fec -= other.fec;
    if (other.first_packet_time_ms != -1 &&
        (other.first_packet_time_ms > this->first_packet_time_ms || this->first_packet_time_ms == -1)) {
        // Prefer the youngest time.
        this->first_packet_time_ms = other.first_packet_time_ms;
    }
    return *this;
}

int64_t RtpStreamDataCounters::TimeSinceFirstPacketInMs(int64_t now_ms) const {
    return (first_packet_time_ms == -1) ? -1 : (now_ms - first_packet_time_ms);
}

size_t RtpStreamDataCounters::MediaPayloadBytes() const {
    // The header and padding bytes of transmitted packet, 
    // retransmitted packets and fec packets are excluded.
    return transmitted.payload_bytes - retransmitted.payload_bytes - fec.payload_bytes;
}

} // namespace naivertc