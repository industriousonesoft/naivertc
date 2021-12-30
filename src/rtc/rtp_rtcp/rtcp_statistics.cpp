#include "rtc/rtp_rtcp/rtcp_statistics.hpp"

namespace naivertc {

// RttStats

RttStats::RttStats() 
    : last_rtt_ms_(0),
      min_rtt_ms_(0),
      max_rtt_ms_(0),
      sum_rtt_ms_(0),
      num_rtts_(0) {}

void RttStats::AddRttMs(int64_t rtt_ms) {
    if (rtt_ms > max_rtt_ms_)
        max_rtt_ms_ = rtt_ms;
    if (num_rtts_ == 0 || rtt_ms < min_rtt_ms_)
        min_rtt_ms_ = rtt_ms;
    last_rtt_ms_ = rtt_ms;
    sum_rtt_ms_ += rtt_ms;
    ++num_rtts_;
}

double RttStats::avg_rtt_ms() const {
    return num_rtts_ > 0 ? static_cast<double>(sum_rtt_ms_) / num_rtts_ 
                         : 0.0;
}

// RtcpReportBlock
RtcpReportBlock::RtcpReportBlock()
    : sender_ssrc(0),
      source_ssrc(0),
      fraction_lost(0),
      packets_lost(0),
      extended_highest_sequence_number(0),
      jitter(0),
      last_sender_report_timestamp(0),
      delay_since_last_sender_report(0) {}

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
