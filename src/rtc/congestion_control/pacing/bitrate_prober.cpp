#include "rtc/congestion_control/pacing/bitrate_prober.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
// The min probe packet size is scaled with the bitrate we're probing at.
// This defines the max min probe packet size, meaning that on high bitrates
// we have a min probe packet size of 200 bytes.
constexpr size_t kMinProbePacketSize = 200; // equal to the min send bitrate 800 kbps.

constexpr TimeDelta kProbeClusterTimeout = TimeDelta::Seconds(5);

}  // namespace

BitrateProber::BitrateProber(const Configuration& config) 
    : config_(config),
      probing_state_(ProbingState::DISABLED),
      next_time_to_probe_(Timestamp::PlusInfinity()),
      total_probe_count_(0),
      total_failed_probe_count_(0) {
    SetEnabled(true);
}

BitrateProber::~BitrateProber() {}

void BitrateProber::SetEnabled(bool enabled) {
    if (enabled) {
        if (probing_state_ == ProbingState::DISABLED) {
            probing_state_ = ProbingState::INACTIVE;
            PLOG_INFO << "Bandwidth probing enabled, set to inactive.";
        }
    } else {
        probing_state_ = ProbingState::DISABLED;
        PLOG_INFO << "Bandwidth probing disabled.";
    }
}

bool BitrateProber::IsProbing() const {
    return probing_state_ == ProbingState::ACTIVE;
}

void BitrateProber::OnIncomingPacket(size_t packet_size) {
    // Don't initialize probing unless we have something large enough
    // to start probing.
    if (probing_state_ == ProbingState::INACTIVE && !clusters_.empty() &&
        packet_size >= std::min(RecommendedMinProbeSize(), kMinProbePacketSize)) {
        // Send next probe immediately.
        next_time_to_probe_ = Timestamp::MinusInfinity();
        probing_state_ = ProbingState::ACTIVE;
    }
}

bool BitrateProber::AddProbeCluster(int cluster_id, 
                                    DataRate bitrate, 
                                    Timestamp at_time) {
    if (probing_state_ == ProbingState::DISABLED || 
        bitrate == DataRate::Zero()) {
        return false;
    }
    
    total_probe_count_++;
    // Remove the clusters which's probing was failed.
    while (!clusters_.empty() &&
            at_time - clusters_.front().created_at > kProbeClusterTimeout) {
        clusters_.pop_front();
        total_failed_probe_count_++;
    }

    ProbeCluster cluster;
    cluster.created_at = at_time;
    cluster.pace_info.probe_cluster.emplace();
    cluster.pace_info.probe_cluster->id = cluster_id;
    cluster.pace_info.probe_cluster->min_probes = config_.min_probe_packets_sent;
    cluster.pace_info.probe_cluster->min_bytes = config_.min_probe_duration.ms() * bitrate.bps() / 8000;
    cluster.pace_info.send_bitrate = bitrate;
    clusters_.push_back(std::move(cluster));

    PLOG_INFO << "Probe cluster (bitrate : min_bytes : min_probes): ("
              << cluster.pace_info.send_bitrate.bps() << " bps : "
              << cluster.pace_info.probe_cluster->min_bytes << " : "
              << cluster.pace_info.probe_cluster->min_probes << ")";

    // If we are already probing, continue doing so.
    // Otherwise set state to inactive and wait for
    // incoming packet to start the probing.
    if (probing_state_ != ProbingState::ACTIVE) {
        probing_state_ = ProbingState::INACTIVE;
    }
    return true;
}

Timestamp BitrateProber::NextTimeToProbe(Timestamp at_time) const {
    // Probing is not active or probing is complete already.
    if (probing_state_ != ProbingState::ACTIVE || clusters_.empty()) {
        return Timestamp::PlusInfinity();
    }
    // It's too late to request next probe.
    if (!config_.abort_delayed_probes && next_time_to_probe_.IsFinite() &&
        at_time - next_time_to_probe_ > config_.max_probe_delay) {
        PLOG_WARNING << "Probe delay too high (exceed " 
                     << config_.max_probe_delay.ms() 
                     << " ms), droping it.";
        return Timestamp::PlusInfinity();
    }
    return next_time_to_probe_;
}

std::optional<PacedPacketInfo> BitrateProber::NextProbeCluster(Timestamp at_time) {
    // Probing is not active or probing is complete already.
    if (probing_state_ != ProbingState::ACTIVE || clusters_.empty()) {
        return std::nullopt;
    }
    // It's too late to request next probe.
    if (config_.abort_delayed_probes && next_time_to_probe_.IsFinite() &&
        at_time - next_time_to_probe_ > config_.max_probe_delay) {
        PLOG_WARNING << "Probe delay too high (exceed " 
                     << config_.max_probe_delay.ms() 
                     << " ms), discarding it.";
        clusters_.pop_front();
        if (clusters_.empty()) {
            probing_state_ = ProbingState::SUSPENDED;
            return std::nullopt;
        }
    }

    PacedPacketInfo info = clusters_.front().pace_info;
    assert(info.probe_cluster.has_value());
    info.probe_cluster->bytes_sent = clusters_.front().sent_bytes;
    return info;
}

size_t BitrateProber::RecommendedMinProbeSize() const {
    // We choose a minimum of twice |min_probe_delta| interval
    // to allow schedule to be feasible.
    if (clusters_.empty()) {
        return 0;
    }
    return 2 * clusters_.front().pace_info.send_bitrate.bps() * config_.min_probe_delta.ms() / 8000;
}

void BitrateProber::OnProbeSent(size_t sent_bytes, Timestamp at_time) {
    if (probing_state_ != ProbingState::ACTIVE || 
        sent_bytes == 0) {
        return;
    }

    if (!clusters_.empty()) {
        auto& cluster = clusters_.front();
        // Check if it's the first time to send probe.
        if (cluster.sent_bytes == 0) {
            cluster.started_at = at_time;
        }
        cluster.sent_bytes += sent_bytes;
        cluster.sent_probes += 1;
        next_time_to_probe_ = CalculateNextProbeTime(cluster);
        assert(cluster.pace_info.probe_cluster);
        // Remove the current cluster if it's probing has done.
        if (cluster.sent_bytes >= cluster.pace_info.probe_cluster->min_bytes &&
            cluster.sent_probes >= cluster.pace_info.probe_cluster->min_probes) {
            clusters_.pop_front();
        }
        if (clusters_.empty()) {
            probing_state_ = ProbingState::SUSPENDED;
        }
    }
}

// Private methods
Timestamp BitrateProber::CalculateNextProbeTime(const ProbeCluster& cluster) const {
    assert(cluster.pace_info.send_bitrate > DataRate::Zero());
    assert(cluster.started_at.IsFinite());

    // Compute the time delta from the cluster start to ensure 
    // probe bitrate stays close to the target bitrate.
    TimeDelta delta = TimeDelta::Millis(cluster.sent_bytes * 8000 / cluster.pace_info.send_bitrate.bps());
    return cluster.started_at + delta;
}
    
} // namespace naivertc
