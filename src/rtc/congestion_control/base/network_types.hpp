#ifndef _RTC_CONGESTION_CONTROL_BASE_NETWORK_TYPES_H_
#define _RTC_CONGESTION_CONTROL_BASE_NETWORK_TYPES_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/units/data_rate.hpp"

#include <optional>
#include <vector>

namespace naivertc {

// ProbeCluster
struct ProbeCluster {     
    int id = -1;
    // The mininum of packet number to estimate probe bitrate.
    size_t min_probes = 0;
    // The mininum of accumulated bytes to estimate probe bitrate.
    size_t min_bytes = 0;
    size_t bytes_sent = 0;
};

// PacedPacketInfo
struct PacedPacketInfo {
    DataRate send_bitrate = DataRate::Zero();
    std::optional<ProbeCluster> probe_cluster = std::nullopt;
};

// SentPacket
struct SentPacket {
    Timestamp send_time = Timestamp::PlusInfinity();
    // Size of packet with overhead up to IP layer.
    size_t size = 0;
    // Size of preceeding packets that are not part of feedback.
    size_t prior_unacked_bytes = 0;
    // Info of the paced packet.
    PacedPacketInfo pacing_info;
    // True if the packet is an audio packet, false for video, padding, RTC etc.
    bool is_audio = false;
    // The unwrapped transport sequence number is unique to any tracked packet.
    int64_t packet_id = -1;
    // Tracked data in flight when the packet was sent, excluding unacked data.
    size_t bytes_in_flight = 0;
};

// ReceivedPacket
struct ReceivedPacket {
    Timestamp send_time = Timestamp::MinusInfinity();
    Timestamp receive_time = Timestamp::MinusInfinity();
    size_t size = 0;
};

// Transport level feedback

struct TransportLossReport {
    Timestamp receive_time = Timestamp::PlusInfinity();
    uint64_t num_packets_lost = 0;
    uint64_t num_packets = 0;
};

// Packet level feedback

// PacketResult
struct PacketResult {
    class ReceiveTimeOrder {
    public:
        bool operator()(const PacketResult& lhs, const PacketResult& rhs);
    };

    bool IsReceived() const { return !recv_time.IsInfinite(); }
    bool IsLost() const { return !IsReceived(); }

    SentPacket sent_packet;
    Timestamp recv_time = Timestamp::PlusInfinity();
};

// TransportPacketsFeedback
struct TransportPacketsFeedback {
    Timestamp receive_time = Timestamp::PlusInfinity();
    Timestamp first_unacked_send_time = Timestamp::PlusInfinity();
    // The receive time of the last acknowledged packet.
    Timestamp last_acked_recv_time = Timestamp::MinusInfinity();
    size_t bytes_in_flight = 0;
    size_t prior_in_flight = 0;
    std::vector<PacketResult> packet_feedbacks;

    // Arrival times for messages without send time information.
    std::vector<Timestamp> sendless_arrival_times;

    // Helper methods
    std::vector<PacketResult> ReceivedPackets() const;
    std::vector<PacketResult> LostPackets() const;
    std::vector<PacketResult> SortedByReceiveTime() const;
};

// Netwrok control

// NetworkEstimate
struct NetworkEstimate
 {
    float loss_rate_ratio = 0;
    TimeDelta rtt = TimeDelta::PlusInfinity();
    TimeDelta bwe_period = TimeDelta::PlusInfinity();
    Timestamp at_time = Timestamp::PlusInfinity();
};

// PacerConfig
struct PacerConfig {
    // Pacer should send at most data_window bytes over time_window duration.
    size_t data_window = 0;
    // Pacer should send at least pad_window bytes over time_window duration.
    size_t pad_window = 0;
    TimeDelta time_window = TimeDelta::PlusInfinity();
    DataRate data_rate() const { return DataRate::BitsPerSec(data_window * 8000 / time_window.ms()); }
    DataRate pad_rate() const { return DataRate::BitsPerSec(pad_window * 8000 / time_window.ms()); }
    Timestamp at_time = Timestamp::PlusInfinity();
};

// ProbeClusterConfig
struct ProbeClusterConfig {
    int32_t id = 0;
    int32_t target_probe_count = 0;
    DataRate target_bitrate = DataRate::Zero();
    TimeDelta target_interval = TimeDelta::Zero();
    Timestamp at_time = Timestamp::PlusInfinity();
};

// TargetTransferRate
struct TargetTransferRate {
    // The estimate on which the target bitrate is based on.
    NetworkEstimate network_estimate;
    DataRate target_bitrate = DataRate::Zero();
    DataRate stable_target_bitrate = DataRate::Zero();
    double cwnd_reduce_ratio = 0;
    Timestamp at_time = Timestamp::PlusInfinity();
};

// NetworkControlUpdate
struct NetworkControlUpdate {
    std::optional<size_t> congestion_window;
    std::optional<PacerConfig> pacer_config;
    std::vector<ProbeClusterConfig> probe_cluster_configs;
    std::optional<TargetTransferRate> target_rate;

    void AppendProbes(std::vector<ProbeClusterConfig> config) {
        if (!config.empty()) {
            probe_cluster_configs.insert(probe_cluster_configs.end(), config.begin(), config.end());
        }
    }
};

// NetworkAvailability
struct NetworkAvailability {
    bool network_available = false;
    Timestamp at_time = Timestamp::PlusInfinity();
};

// TargetBitrateConstraints 
struct TargetBitrateConstraints {
    std::optional<DataRate> min_bitrate;
    std::optional<DataRate> max_bitrate;
    std::optional<DataRate> starting_bitrate;
    Timestamp at_time = Timestamp::PlusInfinity();
};

// NetworkRouteChange
struct NetworkRouteChange {
    TargetBitrateConstraints constraints;
    Timestamp at_time = Timestamp::PlusInfinity();
};

// BitrateAllocationLimits
struct BitrateAllocationLimits {
    // The total minimum send bitrate required by all send streams.
    DataRate min_total_allocated_bitrate = DataRate::Zero();
    // The total maximum allocatable bitrate for all currently availale stream.
    DataRate max_total_allocated_bitrate = DataRate::Zero();
    // The max bitrate to use for padding. The sum of the pre-stream max padding rate.
    DataRate max_padding_bitrate = DataRate::Zero();
};

// StreamsConfig
struct StreamsConfig {
    std::optional<bool> request_alr_probing;
    std::optional<double> pacing_factor;
    BitrateAllocationLimits allocated_bitrate_limits;
    Timestamp at_time = Timestamp::PlusInfinity();
};

// PeriodicUpdate
struct PeriodicUpdate {
    std::optional<size_t> pacer_queue;
    Timestamp at_time = Timestamp::PlusInfinity();
};
    
} // namespace naivertc


#endif