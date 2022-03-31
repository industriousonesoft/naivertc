#ifndef _RTC_RTP_RTCP_RTP_STATISTICS_STRUCTS_H_
#define _RTC_RTP_RTCP_RTP_STATISTICS_STRUCTS_H_

#include "base/defines.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"
#include "rtc/congestion_control/pacing/pacing_types.hpp"

#include <list>
#include <map>
#include <optional>

namespace naivertc {

class RtpPacket;

// RtpState
struct RtpState final {
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
struct RtpPacketCounter final {
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
struct RtpStreamDataCounters final {
    RtpStreamDataCounters();
    ~RtpStreamDataCounters();

    RtpStreamDataCounters& operator+=(const RtpStreamDataCounters& other);
    RtpStreamDataCounters& operator-=(const RtpStreamDataCounters& other);

    std::optional<TimeDelta> TimeSinceFirstPacket(Timestamp at_time) const;

    // Returns the number of bytes corresponding to the actual media payload.
    size_t MediaPayloadBytes() const;

    // The time at which th first packet was sent/received.
    std::optional<Timestamp> first_packet_time;
    // The time at which the last packet was received.
    std::optional<Timestamp> last_packet_received_time;
    RtpPacketCounter transmitted;
    RtpPacketCounter retransmitted;
    RtpPacketCounter fec;
};

// RtpPacketSendInfo
struct RtpPacketSendInfo {
    // Transport sequence number
    uint16_t packet_id = 0;
    uint32_t ssrc = 0;
    uint32_t rtp_timestamp = 0;
    size_t packet_size = 0;
    uint16_t sequence_number;
    std::optional<uint32_t> media_ssrc;
    std::optional<RtpPacketType> packet_type = std::nullopt;
    std::optional<PacedPacketInfo> pacing_info = std::nullopt;
};

// RtpSentPacket
struct RtpSentPacket {
    RtpSentPacket(Timestamp send_time, std::optional<uint16_t> packet_id = std::nullopt) 
        : send_time(send_time),
          packet_id(packet_id) {}
    
    Timestamp send_time = Timestamp::PlusInfinity();;
    // Transport sequence number
    std::optional<uint16_t> packet_id;
    size_t size = 0;
    // Indicates if accounting the packet without packet id 
    // in send side BWE. e.g., used by audio packet.
    bool included_in_allocation = false;
};

// RtpReceiveStats
struct RtpReceiveStats {
    int32_t packets_lost = 0;
    uint32_t jitter = 0;

    // The UTC time based on Unix epoch (1970年1月1日0点0分0秒 UTC).
    std::optional<Timestamp> last_packet_received_posix_time;
    RtpPacketCounter packet_counter;
};

// RtpSendStats
struct RtpSendStats {
    uint32_t packets_sent = 0;
    size_t media_bytes_sent = 0;
    DataRate send_bitrate = DataRate::Zero();
};

} // namespace naivertc

#endif