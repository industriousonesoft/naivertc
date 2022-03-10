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
                                    DataRate target_bitrate, 
                                    Timestamp at_time) {
    if (probing_state_ == ProbingState::DISABLED || 
        target_bitrate == DataRate::Zero()) {
        return false;
    }
    
    total_probe_count_++;
    // Remove the clusters which's probing was failed.
    while (!clusters_.empty() &&
            at_time - clusters_.front().created_at > kProbeClusterTimeout) {
        clusters_.pop_front();
        total_failed_probe_count_++;
    }

    ProbeCluster probe_cluster = {/*id=*/cluster_id, 
                                  /*min_probe=*/config_.min_probe_packets_sent, 
                                  /*min_bytes=*/config_.min_probe_duration * target_bitrate, 
                                  /*target_bitrate=*/target_bitrate};
    ProbeClusterInfo cluster = {probe_cluster};
    cluster.created_at = at_time;
    clusters_.push_back(std::move(cluster));

    PLOG_INFO << "Probe cluster (target_bitrate : min_bytes : min_probes): ("
              << cluster.probe_cluster.target_bitrate.bps() << " bps : "
              << cluster.probe_cluster.min_bytes << " : "
              << cluster.probe_cluster.min_probes << ")";

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
    // NOTE: 此处有两种模式，且该函数常与NextProbeCluster函数组合使用。
    // 模式1：旧模式保留延误的探测包，只是给出警告，返回一个最大时间，然后在NextProbeCluster函数中跳过超时检测，正常发送。
    // 模式2：新模式丢弃延误的探测包，返回正常发送时间，然后在NextProbeCluster函数中因超时而被丢弃。
    // Legacy behavior, just warn about late probe and return as if not probing.
    // It's too late to request next probe.
    if (!config_.abort_delayed_probes && IsProbeDelayed(at_time)) {
        PLOG_WARNING << "Probe delay too high (exceed " 
                     << config_.max_probe_delay.ms() 
                     << " ms), droping it.";
        return Timestamp::PlusInfinity();
    }
    return next_time_to_probe_;
}

std::optional<ProbeCluster> BitrateProber::NextProbeCluster(Timestamp at_time) {
    // Probing is not active or probing is complete already.
    if (probing_state_ != ProbingState::ACTIVE || clusters_.empty()) {
        return std::nullopt;
    }
    // It's too late to request next probe.
    if (config_.abort_delayed_probes && IsProbeDelayed(at_time)) {
        PLOG_WARNING << "Probe delay too high (exceed " 
                     << config_.max_probe_delay.ms() 
                     << " ms), discarding it.";
        clusters_.pop_front();
        if (clusters_.empty()) {
            probing_state_ = ProbingState::SUSPENDED;
            return std::nullopt;
        }
    }
    return clusters_.front().probe_cluster;
}

size_t BitrateProber::RecommendedMinProbeSize() const {
    // We choose a minimum of twice |min_probe_delta| interval
    // to allow schedule to be feasible.
    if (clusters_.empty()) {
        return 0;
    }
    return 2 * clusters_.front().probe_cluster.target_bitrate * config_.min_probe_delta;
}

void BitrateProber::OnProbeSent(size_t sent_bytes, Timestamp at_time) {
    if (probing_state_ != ProbingState::ACTIVE || 
        sent_bytes == 0) {
        return;
    }

    if (!clusters_.empty()) {
        auto& cluster = clusters_.front();
        // Check if it's the first time to send probe.
        if (cluster.probe_cluster.sent_bytes == 0) {
            cluster.started_at = at_time;
        }
        cluster.probe_cluster.sent_bytes += sent_bytes;
        cluster.probe_cluster.sent_probes += 1;
        next_time_to_probe_ = CalculateNextProbeTime(cluster);
        // Remove the current cluster if it's probing has done.
        if (cluster.probe_cluster.IsDone()) {
            clusters_.pop_front();
        }
        if (clusters_.empty()) {
            probing_state_ = ProbingState::SUSPENDED;
        }
    }
}

// Private methods
Timestamp BitrateProber::CalculateNextProbeTime(const ProbeClusterInfo& cluster) const {
    assert(cluster.probe_cluster.target_bitrate > DataRate::Zero());
    assert(cluster.started_at.IsFinite());

    // Compute the time delta from the cluster start to ensure 
    // probe bitrate stays close to the target bitrate.
    TimeDelta delta = cluster.probe_cluster.sent_bytes / cluster.probe_cluster.target_bitrate;
    return cluster.started_at + delta;
}

bool BitrateProber::IsProbeDelayed(Timestamp at_time) const {
    return next_time_to_probe_.IsFinite() && at_time - next_time_to_probe_ > config_.max_probe_delay;
}
    
} // namespace naivertc
