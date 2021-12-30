#ifndef _RTC_RTP_RTCP_RTCP_STATISTICS_H_
#define _RTC_RTP_RTCP_RTCP_STATISTICS_H_

#include "base/defines.hpp"

namespace naivertc {

// RTCPReportBlock
struct RTC_CPP_EXPORT RTCPReportBlock final {
  RTCPReportBlock();
  RTCPReportBlock(uint32_t sender_ssrc,
                  uint32_t source_ssrc,
                  uint8_t fraction_lost,
                  int32_t packets_lost,
                  uint32_t extended_highest_sequence_number,
                  uint32_t jitter,
                  uint32_t last_sender_report_timestamp,
                  uint32_t delay_since_last_sender_report);

    // Fields as described by RFC 3550 6.4.2.
    uint32_t sender_ssrc;  // SSRC of sender of this report.
    uint32_t source_ssrc;  // SSRC of the RTP packet sender.
    uint8_t fraction_lost;
    int32_t packets_lost;  // 24 bits valid.
    uint32_t extended_highest_sequence_number;
    uint32_t jitter;
    uint32_t last_sender_report_timestamp;
    uint32_t delay_since_last_sender_report;
};

// RtcpPacketTypeCounter
struct RTC_CPP_EXPORT RtcpPacketTypeCounter {
    RtcpPacketTypeCounter();

    RtcpPacketTypeCounter& operator+(const RtcpPacketTypeCounter& other);
    RtcpPacketTypeCounter& operator-(const RtcpPacketTypeCounter& other);

    int64_t TimeSinceFirstPacktInMs(int64_t now_ms) const;
   
    int64_t first_packet_time_ms;   // Time when first packet is sent/received.
    uint32_t nack_packets;          // Number of RTCP NACK packets.
    uint32_t fir_packets;           // Number of RTCP FIR packets.
    uint32_t pli_packets;           // Number of RTCP PLI packets.
    uint32_t nack_requests;         // Number of NACKed RTP packets.
    uint32_t unique_nack_requests;  // Number of unique NACKed RTP packets.
};
    
} // namespace naivertc


#endif