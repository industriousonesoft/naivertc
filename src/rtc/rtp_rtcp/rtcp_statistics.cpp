#include "rtc/rtp_rtcp/rtcp_statistics.hpp"

namespace naivertc {

// RTCPReportBlock
RTCPReportBlock::RTCPReportBlock()
    : sender_ssrc(0),
      source_ssrc(0),
      fraction_lost(0),
      packets_lost(0),
      extended_highest_sequence_number(0),
      jitter(0),
      last_sender_report_timestamp(0),
      delay_since_last_sender_report(0) {}

RTCPReportBlock::RTCPReportBlock(uint32_t sender_ssrc,
                                 uint32_t source_ssrc,
                                 uint8_t fraction_lost,
                                 int32_t packets_lost,
                                 uint32_t extended_highest_sequence_number,
                                 uint32_t jitter,
                                 uint32_t last_sender_report_timestamp,
                                 uint32_t delay_since_last_sender_report)
    : sender_ssrc(sender_ssrc),
      source_ssrc(source_ssrc),
      fraction_lost(fraction_lost),
      packets_lost(packets_lost),
      extended_highest_sequence_number(extended_highest_sequence_number),
      jitter(jitter),
      last_sender_report_timestamp(last_sender_report_timestamp),
      delay_since_last_sender_report(delay_since_last_sender_report) {}

// RtcpPacketTypeCounter
RtcpPacketTypeCounter::RtcpPacketTypeCounter() 
    : first_packet_time_ms(-1),
      nack_packets(0),
      fir_packets(0),
      pli_packets(0),
      nack_requests(0),
      unique_nack_requests(0) {}

RtcpPacketTypeCounter& RtcpPacketTypeCounter::operator+(const RtcpPacketTypeCounter& other) {
    nack_packets += other.nack_packets;
    fir_packets += other.fir_packets;
    pli_packets += other.pli_packets;
    nack_requests += other.nack_requests;
    unique_nack_requests += other.unique_nack_requests;
    if (other.first_packet_time_ms != -1 &&
       (other.first_packet_time_ms < first_packet_time_ms ||
        first_packet_time_ms == -1)) {
        // Use oldest time.
        first_packet_time_ms = other.first_packet_time_ms;
    }
    return *this;
}
    
RtcpPacketTypeCounter& RtcpPacketTypeCounter::operator-(const RtcpPacketTypeCounter& other) {
    nack_packets -= other.nack_packets;
    fir_packets -= other.fir_packets;
    pli_packets -= other.pli_packets;
    nack_requests -= other.nack_requests;
    unique_nack_requests -= other.unique_nack_requests;
    if (other.first_packet_time_ms != -1 &&
        (other.first_packet_time_ms > first_packet_time_ms ||
        first_packet_time_ms == -1)) {
        // Use youngest time.
        first_packet_time_ms = other.first_packet_time_ms;
    }
    return *this;
}

int64_t RtcpPacketTypeCounter::TimeSinceFirstPacktInMs(int64_t now_ms) const {
    return (first_packet_time_ms == -1) ? -1 : (now_ms - first_packet_time_ms);
}
    
} // namespace naivertc
