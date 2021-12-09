#include "rtc/congestion_controller/goog_cc/probe_bitrate_estimator.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
// The minumum number of probes we need to receive feedback about in percent
// in order to have a valid estimate.
constexpr double kMinReceivedProbesRatio = 0.8f;

// The minumum number of bytes we need to receive feedback about in percent
// in order to have a valid estimate.
constexpr double kMinReceivedBytesRatio = 0.8f;

// The maximum |receive rate| / |send rate| ratio for a valid estimate.
constexpr float kMaxValidRatio = 2.0f;

// The minimum |receive rate| / |send rate| ratio assuming that the link is
// not saturated, i.e. we assume that we will receive at least
// kMinRatioForUnsaturatedLink * |send rate| if |send rate| is less than the
// link capacity.
constexpr float kMinRatioForUnsaturatedLink = 0.9f;

// The target utilization of the link. If we know true link capacity
// we'd like to send at 95% of that rate.
constexpr float kTargetUtilizationFraction = 0.95f;

// The maximum time period over which the cluster history is retained.
// This is also the maximum time period beyond which a probing burst is not
// expected to last.
constexpr TimeDelta kMaxClusterHistory = TimeDelta::Seconds(1);

// The maximum time interval between first and the last probe on a cluster
// on the sender side as well as the receive side.
constexpr TimeDelta kMaxProbeInterval = TimeDelta::Seconds(1);

}  // namespace

ProbeBitrateEstimator::ProbeBitrateEstimator() 
    : estimated_bitrate_(std::nullopt) {}
    
ProbeBitrateEstimator::~ProbeBitrateEstimator() = default;

std::optional<DataRate> ProbeBitrateEstimator::HandleProbeAndEstimateBitrate(const PacketResult& packet_feedback) {
    assert(packet_feedback.sent_packet.pacing_info.probe_cluster.has_value());
    assert(packet_feedback.sent_packet.pacing_info.probe_cluster->min_bytes > 0);
    assert(packet_feedback.sent_packet.pacing_info.probe_cluster->min_bytes > 0);

    int cluster_id = packet_feedback.sent_packet.pacing_info.probe_cluster->id;

    EraseOldCluster(packet_feedback.recv_time);

    AggregatedCluster* curr_cluster = &clusters_[cluster_id];

    if (packet_feedback.sent_packet.send_time < curr_cluster->first_send_time) {
        curr_cluster->first_send_time = packet_feedback.sent_packet.send_time;
    }
    if (packet_feedback.sent_packet.send_time > curr_cluster->last_send_time) {
        curr_cluster->last_send_time = packet_feedback.sent_packet.send_time;
        curr_cluster->last_send_size = packet_feedback.sent_packet.size;
    }
    if (packet_feedback.recv_time < curr_cluster->first_recv_time) {
        curr_cluster->first_recv_time = packet_feedback.recv_time;
        curr_cluster->first_recv_size = packet_feedback.sent_packet.size;
    }
    if (packet_feedback.recv_time > curr_cluster->last_recv_time) {
        curr_cluster->last_recv_time = packet_feedback.recv_time;
    }
    curr_cluster->total_size += packet_feedback.sent_packet.size;
    curr_cluster->num_probes += 1;

    int min_probes = packet_feedback.sent_packet.pacing_info.probe_cluster->min_bytes * kMinReceivedProbesRatio;
    size_t min_size = packet_feedback.sent_packet.pacing_info.probe_cluster->min_bytes * kMinReceivedBytesRatio;

    if (curr_cluster->num_probes < min_probes || curr_cluster->total_size < min_size) {
        return std::nullopt;
    }

    TimeDelta send_interval = curr_cluster->last_send_time - curr_cluster->first_send_time;
    TimeDelta recv_interval = curr_cluster->last_recv_time - curr_cluster->first_recv_time;

    // Invalid send/receive interval.
    if (send_interval <= TimeDelta::Zero() || send_interval > kMaxProbeInterval ||
        recv_interval <= TimeDelta::Zero() || recv_interval > kMaxProbeInterval) {
        PLOG_INFO << "Probing unsuccessful, invalid send/receive interval"
                  << " [cluster id: " << cluster_id << "]"
                  << " [send interval: " << send_interval.ms() << " ms]"
                  << " [receive interval: " << recv_interval.ms() << " ms]";
        return std::nullopt;
    }
    // The size of the last sent packet should not be included when calculating the send bitrate,
    // since the `send_interval` dose not include the time taken to actually send the last packet.
    assert(curr_cluster->total_size > curr_cluster->last_send_size);
    size_t send_size = curr_cluster->total_size - curr_cluster->last_send_size;
    DataRate send_bitrate = DataRate::BytesPerSec(send_size * 1000 / send_interval.ms());

    // The size of the first received packet should not be included when calculating the receive bitrate,
    // since the `recv_interval` dose not include the time taken to actually receive the first packet.
    size_t recv_size = curr_cluster->total_size - curr_cluster->first_recv_size;
    DataRate recv_bitrate = DataRate::BytesPerSec(recv_size * 1000 / recv_interval.ms());

    double ratio = recv_bitrate / send_bitrate;
    if (ratio > kMaxValidRatio) {
        PLOG_INFO << "Probing unsuccessful, receive/send ratio too high \n"
                  << " [cluster id: " << cluster_id << "] " 
                  << "[send: " <<send_size << " / " << send_interval.ms() << " ms = "
                  << send_bitrate.kbps<double>() << " kbps]"
                  << " [receive: " << recv_size << " / " << recv_interval.ms() << " ms = "
                  << recv_bitrate.kbps<double>() << " kbps]"
                  << " [ratio: " << ratio << " > kMaxValidRatio ("
                  << kMaxValidRatio << ")]";
        return std::nullopt;
    }

    PLOG_INFO << "Probing successful, \n"
              << " [cluster id: " << cluster_id << "] " 
              << "[send: " <<send_size << " / " << send_interval.ms() << " ms = "
              << send_bitrate.kbps<double>() << " kbps]"
              << " [receive: " << recv_size << " / " << recv_interval.ms() << " ms = "
              << recv_bitrate.kbps<double>() << " kbps]"
              << " [ratio: " << ratio << "]";

    DataRate ret = std::min(send_bitrate, recv_bitrate);

    // If we're receiving at significantly lower bitrate than we were sending at,
    // it suggests that we've found the true capacity of the link.
    // In this case, set the target bitrate siligtly lower to not immediately overuse.
    if (recv_bitrate.kbps<double>() < kMinRatioForUnsaturatedLink * send_bitrate.bps<double>()) {
        assert(recv_bitrate < send_bitrate);
        ret = DataRate::BitsPerSec(kTargetUtilizationFraction * recv_bitrate.bps<double>());
    }
    estimated_bitrate_ = ret;
    return ret;

}

std::optional<DataRate> ProbeBitrateEstimator::FetchAndResetLastEstimateBitrate() {
    std::optional<DataRate> estimated_bitrate = estimated_bitrate_;
    estimated_bitrate_.reset();
    return estimated_bitrate;
}

// Private methods
void ProbeBitrateEstimator::EraseOldCluster(Timestamp timestamp) {
    for (auto it = clusters_.begin(); it != clusters_.end();) {
        if (it->second.last_recv_time + kMaxClusterHistory < timestamp) {
            it = clusters_.erase(it);
        } else {
            ++it;
        }
    }
}
    
} // namespace naivertc
