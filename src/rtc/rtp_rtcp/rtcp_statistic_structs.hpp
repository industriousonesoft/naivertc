#ifndef _RTC_RTP_RTCP_RTCP_STATISTICS_H_
#define _RTC_RTP_RTCP_RTCP_STATISTICS_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/time/ntp_time.hpp"

namespace naivertc {

// RttStats
class RttStats {
public:
    RttStats();

    int64_t last_rtt_ms() const { return last_rtt_ms_; }
    int64_t min_rtt_ms() const { return min_rtt_ms_; }
    int64_t max_rtt_ms() const { return max_rtt_ms_; }
    int64_t sum_rtt_ms() const { return sum_rtt_ms_; }
    double avg_rtt_ms() const;
    size_t num_rtts() const { return num_rtts_; }
   
    void AddRttMs(int64_t rtt_ms);

private:
    int64_t last_rtt_ms_ = -1;
    int64_t min_rtt_ms_ = -1;
    int64_t max_rtt_ms_ = -1;
    int64_t sum_rtt_ms_ = -1;
    size_t num_rtts_ = 0;
};

// RtcpReportBlock
struct RTC_CPP_EXPORT RtcpReportBlock final {
  RtcpReportBlock();
  RtcpReportBlock(uint32_t sender_ssrc,
                  uint32_t source_ssrc,
                  uint8_t fraction_lost,
                  int32_t packets_lost,
                  uint32_t extended_highest_sequence_number,
                  uint32_t jitter,
                  uint32_t last_sender_report_timestamp,
                  uint32_t delay_since_last_sender_report);

    // Fields as described by RFC 3550 6.4.2.
    uint32_t sender_ssrc = 0;  // SSRC of sender of this report.
    uint32_t source_ssrc = 0;  // SSRC of the RTP packet sender.
    uint8_t fraction_lost = 0;
    int32_t packets_lost = 0;  // 24 bits valid.
    uint32_t extended_highest_sequence_number = 0;
    uint32_t jitter = 0;
    uint32_t last_sender_report_timestamp = 0;
    uint32_t delay_since_last_sender_report = 0;
};

// RtcpPacketTypeCounter
struct RTC_CPP_EXPORT RtcpPacketTypeCounter {
    RtcpPacketTypeCounter();

    RtcpPacketTypeCounter& operator+(const RtcpPacketTypeCounter& other);
    RtcpPacketTypeCounter& operator-(const RtcpPacketTypeCounter& other);

    int64_t TimeSinceFirstPacktInMs(int64_t now_ms) const;
   
    int64_t first_packet_time_ms = -1;   // Time when first packet is sent/received.
    uint32_t nack_packets = 0;           // Number of RTCP NACK packets.
    uint32_t fir_packets = 0;            // Number of RTCP FIR packets.
    uint32_t pli_packets = 0;            // Number of RTCP PLI packets.
    uint32_t nack_requests = 0;          // Number of NACKed RTP packets.
    uint32_t unique_nack_requests = 0;   // Number of unique NACKed RTP packets.
};

// RtcpSenderReportStats
struct RTC_CPP_EXPORT RtcpSenderReportStats {
    NtpTime send_ntp_time;
    uint32_t send_rtp_time = 0;
    NtpTime arrival_ntp_time;
    uint32_t packet_sent = 0;
    uint64_t bytes_sent = 0;
    uint64_t reports_count = 0;
};
    
} // namespace naivertc

#endif