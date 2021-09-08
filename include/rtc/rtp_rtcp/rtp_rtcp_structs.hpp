#ifndef _RTC_RTP_RTCP_RTP_RTCP_STRUCTS_H_
#define _RTC_RTP_RTCP_RTP_RTCP_STRUCTS_H_

#include "base/defines.hpp"

#include <list>
#include <map>

namespace naivertc {

class RtpPacket;
class BitRate;

// ========= RTCP Structs =========
// RTCPReportBlock
struct RTC_CPP_EXPORT RTCPReportBlock final {
  RTCPReportBlock()
      : sender_ssrc(0),
        source_ssrc(0),
        fraction_lost(0),
        packets_lost(0),
        extended_highest_sequence_number(0),
        jitter(0),
        last_sender_report_timestamp(0),
        delay_since_last_sender_report(0) {}

  RTCPReportBlock(uint32_t sender_ssrc,
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
typedef std::list<RTCPReportBlock> ReportBlockList;

// ========= RTP Structs =========
// RtpState
struct RTC_CPP_EXPORT RtpState final {
    RtpState();
    ~RtpState();

    uint16_t sequence_num;
    uint32_t start_timestamp;
    uint32_t timestamp;
    int64_t capture_time_ms;
    int64_t last_timestamp_time_ms;
    bool ssrc_has_acked;
};

// RtpPacketCounter
struct RTC_CPP_EXPORT RtpPacketCounter final {
    RtpPacketCounter();
    explicit RtpPacketCounter(const RtpPacket& packet);
    ~RtpPacketCounter();

    bool operator==(const RtpPacketCounter& other) const;
    RtpPacketCounter& operator+=(const RtpPacketCounter& other);

    size_t TotalBytes() const;

    size_t header_bytes;   // Number of bytes used by RTP headers.
    size_t payload_bytes;  // Payload bytes, excluding RTP headers and padding.
    size_t padding_bytes;  // Number of padding bytes.
    uint32_t packets;      // Number of packets.
};

// RtpSentCounters
struct RTC_CPP_EXPORT RtpSentCounters final {
    RtpSentCounters();
    ~RtpSentCounters();

    RtpSentCounters& operator+=(const RtpSentCounters& other);

    RtpPacketCounter transmitted;
    RtpPacketCounter retransmitted;
    RtpPacketCounter fec;
};

class RTC_CPP_EXPORT RtpSentStatisticsObserver {
public:
    virtual ~RtpSentStatisticsObserver() = default;
    virtual void RtpSentCountersUpdated(const RtpSentCounters& rtp_sent_counters, const RtpSentCounters& rtx_sent_counters) = 0;
    virtual void RtpSentBitRateUpdated(const BitRate bit_rate) = 0;
};

} // namespace naivertc

#endif