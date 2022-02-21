#include "rtc/congestion_controller/goog_cc/probe_controller.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

// The minimum number probing packets used.
constexpr int kMinProbePacketsSent = 5;

// The minimum probing duration in ms.
constexpr TimeDelta kMinProbeInterval = TimeDelta::Millis(15);

// Maximum waiting time from the time of initiating probing to getting
// the measured results back.
constexpr TimeDelta kMaxWaitingTimeForProbingResult = TimeDelta::Millis(1000);

// Default probing bitrate limit. Applied only when the application didn't
// specify max bitrate.
constexpr DataRate kDefaultMaxProbingBitrate = DataRate::BitsPerSec(5000'000);

// If the bitrate drops to a factor |kBitrateDropThreshold| or lower
// and we recover within |kBitrateDropTimeoutMs|, then we'll send
// a probe at a fraction |kProbeFractionAfterDrop| of the original bitrate.
constexpr double kBitrateDropThreshold = 0.66;
constexpr TimeDelta kBitrateDropTimeout = TimeDelta::Millis(5000);
constexpr double kProbeFractionAfterDrop = 0.85;

// Timeout for probing after leaving ALR. If the bitrate drops significantly,
// (as determined by the delay based estimator) and we leave ALR, then we will
// send a probe if we recover within |kLeftAlrTimeoutMs| ms.
constexpr TimeDelta kAlrEndedTimeout = TimeDelta::Millis(3000);

// The expected uncertainty of probe result (as a fraction of the target probe
// This is a limit on how often probing can be done when there is a BW
// drop detected in ALR.
constexpr TimeDelta kMinTimeBetweenAlrProbes = TimeDelta::Millis(5000);

// bitrate). Used to avoid probing if the probe bitrate is close to our current
// estimate.
constexpr double kProbeUncertainty = 0.05;
    
} // namespace

// ProbeController
ProbeController::ProbeController(const Configuration& config) 
    : config_(config) {}
    
ProbeController::~ProbeController() {}

void ProbeController::set_enable_periodic_alr_probing(bool enable) {
    enable_periodic_alr_probing_ = enable;
}

void ProbeController::set_alr_start_time(std::optional<Timestamp> start_time) {
    alr_start_time_ = start_time;
}

void ProbeController::set_alr_end_time(Timestamp end_time) {
    alr_end_time_.emplace(end_time);
}

std::vector<ProbeClusterConfig> ProbeController::OnBitrateConstraints(DataRate min_bitrate,
                                                                      DataRate start_bitrate,
                                                                      DataRate max_bitrate,
                                                                      Timestamp at_time) {
    if (start_bitrate.IsZero()) {
        start_bitrate_ = min_bitrate;
    } if (start_bitrate > DataRate::Zero()) {
        start_bitrate_ = start_bitrate;
        estimated_bitrate_ = start_bitrate;
    }

    auto old_max_bitrate = max_bitrate_;
    max_bitrate_ = max_bitrate;

    switch (probing_state_) {
    case ProbingState::NEW:
        // Initiation of probing to estimate initial channel capacity.
        return InitExponentialProbing(at_time);
        break;
    case ProbingState::WAITING:
        break;
    case ProbingState::DONE:
        // If the new max bitrate is higher than both the current max bitrate
        // and the estimate, we initiate probing to estimate current channel
        // capacity.
        if (!estimated_bitrate_.IsZero() &&
            old_max_bitrate < max_bitrate_ &&
            estimated_bitrate_ < max_bitrate_) {
            // The assumption is that if we jump more than 20% in the bandwidth
            // esitmate or if the bandwidth estimate is within 90% of the new
            // max bitrate then the probing attempt was considered as success.
            MidCallProbing mid_call_probing;
            mid_call_probing.probing_state = ProbingState::WAITING;
            mid_call_probing.bitrate_to_probe = max_bitrate_;
            mid_call_probing.success_threshold = std::min(estimated_bitrate_ * 1.2, max_bitrate * 0.9);
            mid_call_probing_.emplace(std::move(mid_call_probing));
            return InitProbing({max_bitrate}, false, at_time);
        }
        break;
    }
    return std::vector<ProbeClusterConfig>();
}

std::vector<ProbeClusterConfig> ProbeController::OnMaxTotalAllocatedBitrate(DataRate max_total_allocated_bitrate,
                                                                            Timestamp at_time) {
    const bool in_alr = InAlr();
    const bool allow_allocation_probe = in_alr;

    // Conditions:
    // 1. We are in ALR state.
    // 2. The recent probing is done.
    // 3. We get a new |max_total_allocated_bitrate|.
    // 4. We have a valid estimate already.
    // 5. The max bitrate is invalid or the estimate is less than the max.
    // 6. The estimate is less than the |max_total_allocated_bitrate|.
    if (allow_allocation_probe &&
        probing_state_ == ProbingState::DONE &&
        max_total_allocated_bitrate_ != max_total_allocated_bitrate &&
        !estimated_bitrate_.IsZero() &&
        (max_bitrate_.IsInfinite() || estimated_bitrate_ < max_bitrate_) &&
        estimated_bitrate_ < max_total_allocated_bitrate) {
        
        if (config_.first_allocation_probe_scale <= 0) {
            return std::vector<ProbeClusterConfig>();
        }

        // First probe bitrate.
        DataRate first_probe_bitrate = max_total_allocated_bitrate * config_.first_allocation_probe_scale;
        DataRate probe_cap = config_.allocation_probe_cap;
        first_probe_bitrate = std::min(first_probe_bitrate, probe_cap);
        std::vector<DataRate> pending_probes = {first_probe_bitrate};

        // Second probe bitrate.
        if (config_.second_allocation_probe_scale > 0) {
            DataRate second_probe_bitrate = max_total_allocated_bitrate * config_.second_allocation_probe_scale;
            second_probe_bitrate = std::min(second_probe_bitrate, probe_cap);
            if (second_probe_bitrate > first_probe_bitrate) {
                pending_probes.push_back(second_probe_bitrate);
            }
        }
        return InitProbing(pending_probes, config_.allocation_allow_further_probing, at_time);
    } else {
        max_total_allocated_bitrate_ = max_total_allocated_bitrate;
        return std::vector<ProbeClusterConfig>();
    }
}

std::vector<ProbeClusterConfig> ProbeController::OnEstimatedBitrate(DataRate estimate,
                                                                    Timestamp at_time) {
    // Check if the MidCallProbing is successful.
    if (mid_call_probing_->probing_state == ProbingState::WAITING &&
        estimate > mid_call_probing_->success_threshold) {
        PLOG_VERBOSE << "MidCallProbing is successful: probing bitrate=" 
                     << mid_call_probing_->bitrate_to_probe.kbps()
                     << " kbps, probed birate=" 
                     << estimate.kbps() << " kbps.";
        mid_call_probing_.reset();
    }
    std::vector<ProbeClusterConfig> pending_probes;
    // Check if we can continue probing further.
    if (probing_state_ == ProbingState::WAITING && 
        min_bitrate_to_probe_further_) {
        PLOG_INFO << "Measured bitrate=" << estimate.bps() 
                  << " bps, minimum to probe furter="
                  << min_bitrate_to_probe_further_->bps();
        // Continue probing if the current probing results indicate
        // channel has greater capacity.
        if (estimate > *min_bitrate_to_probe_further_) {
            auto further_probe_bitrate = estimate * config_.further_exponential_probe_scale;
            pending_probes = InitProbing({further_probe_bitrate}, true, at_time);
        }
    }

    if (estimate < estimated_bitrate_ * kBitrateDropThreshold) {
        time_last_large_drop_ = at_time;
        bitrate_before_last_large_drop_ = estimated_bitrate_;
    }
    estimated_bitrate_ = estimate;
    return pending_probes;
}

std::vector<ProbeClusterConfig> ProbeController::OnPeriodicProcess(Timestamp at_time) {
    // The current probing is timeout.
    if (at_time - time_last_probing_initiated_ > kMaxWaitingTimeForProbingResult /* 1s */) {
        mid_call_probing_.reset();
        if (probing_state_ == ProbingState::WAITING) {
            PLOG_WARNING << "The current probing is timeout.";
            probing_state_ = ProbingState::DONE;
            min_bitrate_to_probe_further_.reset();
        }
    }

    if (enable_periodic_alr_probing_ && probing_state_ == ProbingState::DONE) {
        // Probing peridiclly when in ALR state.
        if (alr_start_time_ && !estimated_bitrate_.IsZero()) {
            auto time_to_next_probe = std::max(*alr_start_time_, time_last_probing_initiated_) + config_.alr_probing_interval;
            // Check if it's time to probe.
            if (at_time >= time_to_next_probe) {
                return InitProbing({estimated_bitrate_ * config_.alr_probe_scale}, true, at_time);
            }
        }
    }
    return std::vector<ProbeClusterConfig>();
}

std::vector<ProbeClusterConfig> ProbeController::RequestProbe(Timestamp at_time) {
    // Called once we have returned to normal state after a large drop in
    // estimated bandwidth. The current response is to initiate a single 
    // probe session (if not already probing) at the previous bitrate.
    //
    // If the probe session fails, the assumption is that this drop was
    // a real one from a compeitng flow or a newwork change.
    const bool in_alr = InAlr();
    const bool alr_ended_recently = (alr_end_time_ && 
                                     at_time - *alr_end_time_ < kAlrEndedTimeout);
    if (in_alr || alr_ended_recently) {
        if (probing_state_ == ProbingState::DONE) {
            // Compute the suggested bitrate to probe.
            DataRate bitrate_to_probe = bitrate_before_last_large_drop_ * kProbeFractionAfterDrop;
            DataRate min_expected_probe_bitrate = bitrate_to_probe * (1 - kProbeUncertainty);
            auto interval_since_last_drop = at_time - time_last_large_drop_;
            auto interval_since_last_request = at_time - time_last_probe_request_;
            if (min_expected_probe_bitrate > estimated_bitrate_ &&
                interval_since_last_drop < kBitrateDropTimeout &&
                interval_since_last_request > kMinTimeBetweenAlrProbes) {
                PLOG_INFO << "Detected big bandwidth drop, start probing.";
                time_last_probe_request_ = at_time;
                return InitProbing({bitrate_to_probe}, false, at_time);
            }
        }
    }
    return std::vector<ProbeClusterConfig>();
}

void ProbeController::Reset(Timestamp at_time) {
    probing_state_ = ProbingState::NEW;

    start_bitrate_ = DataRate::Zero();
    estimated_bitrate_ = DataRate::Zero();
    max_bitrate_ = DataRate::Zero();
    max_total_allocated_bitrate_ = DataRate::Zero();

    time_last_probing_initiated_ = Timestamp::Zero();
    time_last_large_drop_ = at_time;
    time_last_probe_request_ = at_time;

    bitrate_before_last_large_drop_ = DataRate::Zero();

    min_bitrate_to_probe_further_ = std::nullopt;

    mid_call_probing_ = std::nullopt;

    alr_end_time_.reset();
}

// Private methods
std::vector<ProbeClusterConfig> ProbeController::InitProbing(std::vector<DataRate> bitrates_to_probe,
                                                             bool probe_further, 
                                                             Timestamp at_time) {
    DataRate max_probe_bitrate = !max_bitrate_.IsZero() ? max_bitrate_ 
                                                            : kDefaultMaxProbingBitrate;
    if (config_.limit_probes_with_allocatable_bitrate &&
        max_total_allocated_bitrate_ > DataRate::Zero()) {
        // If a max allocated bitrate has been configured, allow probing up to 2x
        // that rate. This allows some overhead to account for bursty streams,
        // which otherwise would have to ramp up when the overshoot is already in
        // progress.
        // It also avoids minor quality reduction caused by probes often being
        // received at slightly less than the target probe bitrate.
        max_probe_bitrate = std::min(max_probe_bitrate, max_total_allocated_bitrate_ * 2);
    }
    std::vector<ProbeClusterConfig> pending_probes;
    DataRate max_bitrate = DataRate::Zero();
    for (const auto& bitrate : bitrates_to_probe) {
        assert(bitrate.bps() >= 0);
        ProbeClusterConfig config;
        config.at_time = at_time;
        config.target_interval = kMinProbeInterval;
        config.target_probe_count = kMinProbePacketsSent;
        config.id = next_probe_cluster_id_++;
        if (bitrate > max_probe_bitrate) {
            config.target_bitrate = max_probe_bitrate;
            // No need to probe further as we will 
            // probe the max probe bitrate.
            probe_further = false;
        } else {
            config.target_bitrate = bitrate;
        }
        max_bitrate = std::max(max_bitrate, bitrate);
        pending_probes.push_back(std::move(config));
    }
    time_last_probing_initiated_ = at_time;
    // Need to probe further.
    if (probe_further) {
        probing_state_ = ProbingState::WAITING;
        // Set the max birate as the min birate to probe further
        if (!max_bitrate.IsZero()) {
            min_bitrate_to_probe_further_.emplace(max_bitrate * config_.further_probe_scale);
        }
    } else {
        probing_state_ = ProbingState::DONE;
        min_bitrate_to_probe_further_.reset();
    }
    return pending_probes;
}

std::vector<ProbeClusterConfig> ProbeController::InitExponentialProbing(Timestamp at_time) {
    assert(probing_state_ == ProbingState::NEW);
    assert(!start_bitrate_.IsZero());

    std::vector<DataRate> bitrates_to_probe;
    // Applies the first exponential probe scale as it's available.
    if (config_.first_exponential_probe_scale > 0) {
        bitrates_to_probe.push_back(start_bitrate_ * config_.first_exponential_probe_scale);
    }
    // Applies the second exponential probe scale as it's available.
    if (config_.second_exponential_probe_scale > 0) {
        bitrates_to_probe.push_back(start_bitrate_ * config_.second_exponential_probe_scale);
    }
    return InitProbing(bitrates_to_probe, true, at_time);
}

bool ProbeController::InAlr() const {
    return alr_start_time_.has_value();
}
    
} // namespace naivertc