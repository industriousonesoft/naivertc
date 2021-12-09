#ifndef _RTC_CONGESTION_CONTROLLER_NETWORK_TYPES_H_
#define _RTC_CONGESTION_CONTROLLER_NETWORK_TYPES_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/units/data_rate.hpp"

#include <optional>
#include <vector>

namespace naivertc {

// ProbeCluster
struct RTC_CPP_EXPORT ProbeCluster {
    int id = -1;
    int min_probes = -1;
    int min_bytes = -1;
    int bytes_sent = 0;
};

// PacedPacketInfo
struct RTC_CPP_EXPORT PacedPacketInfo {
    int send_bitrate_bps = -1;
    std::optional<ProbeCluster> probe_cluster = std::nullopt;
};

// SendPacket
struct RTC_CPP_EXPORT SendPacket {
    Timestamp send_time = Timestamp::PlusInfinity();
    // Size of packet with overhead up to IP layer.
    size_t size = 0;
    // Size of preceeding packets that are not part of feedback.
    size_t prior_unacked_bytes = 0;
    // Info of the paced packet.
    PacedPacketInfo pacing_info;
    // True if the packet is an audio packet, false for video, padding, RTC etc.
    bool is_audio = false;
    // The unwrapped sequence number is unique to any tracked packet.
    int64_t sequence_number = -1;
    // Tracked data in flight when the packet was sent, excluding unacked data.
    size_t bytes_in_flight = 0;
};

// PacketResult
struct RTC_CPP_EXPORT PacketResult {
    class ReceiveTimeOrder {
    public:
        bool operator()(const PacketResult& lhs, const PacketResult& rhs);
    };

    bool IsReceived() const { return !recv_time.IsInfinite(); }

    SendPacket sent_packet;
    Timestamp recv_time = Timestamp::PlusInfinity();
};

// TransportPacketsFeedback
struct RTC_CPP_EXPORT TransportPacketsFeedback {
    Timestamp feedback_time = Timestamp::PlusInfinity();
    Timestamp first_unacked_send_time = Timestamp::PlusInfinity();
    size_t bytes_in_flight = 0;
    size_t prior_in_flight = 0;
    std::vector<PacketResult> packet_feedbacks;

    // Arrival times for messages without send time information.
    std::vector<Timestamp> sendless_arrival_times;

    // Helper methods
    std::vector<PacketResult> ReceivedWithSendInfo() const;
    std::vector<PacketResult> LostWithSendInfo() const;
    std::vector<PacketResult> PacketsWithFeedback() const;
    std::vector<PacketResult> SortedByReceiveTime() const;
};
    
} // namespace naivertc


#endif