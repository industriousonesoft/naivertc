#ifndef _RTC_RTP_RTCP_RTP_STATISTICS_STRUCTS_H_
#define _RTC_RTP_RTCP_RTP_STATISTICS_STRUCTS_H_

#include "base/defines.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"

#include <list>
#include <map>

namespace naivertc {

class RtpPacket;

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
    RtpPacketCounter& operator-=(const RtpPacketCounter& other);

    void AddPacket(const RtpPacket& packet);

    size_t TotalBytes() const;

    size_t header_bytes;   // Number of bytes used by RTP headers.
    size_t payload_bytes;  // Payload bytes, excluding RTP headers and padding.
    size_t padding_bytes;  // Number of padding bytes.
    uint32_t num_packets;  // Number of packets.
};

// RtpStreamDataCounters
struct RTC_CPP_EXPORT RtpStreamDataCounters final {
    RtpStreamDataCounters();
    ~RtpStreamDataCounters();

    RtpStreamDataCounters& operator+=(const RtpStreamDataCounters& other);
    RtpStreamDataCounters& operator-=(const RtpStreamDataCounters& other);

    int64_t TimeSinceFirstPacketInMs(int64_t now_ms) const;

    // Returns the number of bytes corresponding to the actual media payload.
    size_t MediaPayloadBytes() const;

    // The time at which th first packet was sent/received.
    int64_t first_packet_time_ms;
    // The time at which the last packet was received.
    std::optional<int64_t> last_packet_received_time_ms;
    RtpPacketCounter transmitted;
    RtpPacketCounter retransmitted;
    RtpPacketCounter fec;
};

// RtpTransportFeedback
struct RTC_CPP_EXPORT RtpTransportFeedback {
    uint32_t ssrc = 0;
    uint16_t packet_id = 0;
    uint16_t seq_num = 0;
    uint32_t rtp_timestamp = 0;
    size_t packet_size = 0;
    std::optional<uint32_t> media_ssrc;
    std::optional<RtpPacketType> packet_type;
    std::optional<uint16_t> retransmitted_seq_num;
};

// RtpReceiveStats
struct RTC_CPP_EXPORT RtpReceiveStats {
    int32_t packets_lost = 0;
    uint32_t jitter = 0;

    std::optional<int64_t> last_packet_received_time_ms;
    RtpPacketCounter packet_counter;
};

// RtpSendFeedback
struct RTC_CPP_EXPORT RtpSendFeedback {
    uint32_t packets_sent = 0;
    size_t media_bytes_sent = 0;
    DataRate send_bitrate = DataRate::Zero();
};

} // namespace naivertc

#endif