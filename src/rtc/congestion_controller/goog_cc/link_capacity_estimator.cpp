#include "rtc/congestion_controller/goog_cc/link_capacity_estimator.hpp"

namespace naivertc {

LinkCapacityEstimator::LinkCapacityEstimator() 
    : estimate_kbps_(std::nullopt),
      variance_kbps_(0.4) {}

LinkCapacityEstimator::~LinkCapacityEstimator() = default;

void LinkCapacityEstimator::Reset() {
    estimate_kbps_.reset();
}

void LinkCapacityEstimator::OnOveruseDetected(DataRate acknowledged_rate) {
    // It is RECOMMENDED to measure this average and standard deviation with an
    // exponential moving average with the smoothing factor 0.95, as it is
    // expected that this average covers multiple occasions at which we are
    // in the Decrease state. 
    Update(acknowledged_rate, 0.95);
}

void LinkCapacityEstimator::OnProbeRate(DataRate probe_rate) {
    Update(probe_rate, 0.5);
}

std::optional<DataRate> LinkCapacityEstimator::UpperBound() const {
    if (estimate_kbps_) {
        // The uppper bound is defined as three standard deviations above the average
        // max bitrate.
        return DataRate::KilobitsPerSec(estimate_kbps_.value() + 3 * EstimatedStdDev());
    }
    return std::nullopt;
}

std::optional<DataRate> LinkCapacityEstimator::LowerBound() const {
    if (estimate_kbps_) {
        // The uppper bound is defined as three standard deviations below the average
        // max bitrate, but not negetive.
        return DataRate::KilobitsPerSec(std::max<double>(0.0, estimate_kbps_.value() - 3 * EstimatedStdDev()));
    }
    return std::nullopt;
}

std::optional<DataRate> LinkCapacityEstimator::Estimate() const {
    if (estimate_kbps_) {
        return DataRate::KilobitsPerSec(estimate_kbps_.value());
    }
    return std::nullopt;
}

// Private methods
void LinkCapacityEstimator::Update(DataRate capacity_sample, double smoothing_coeff) {
    double sample_kbps = capacity_sample.kbps();
    if (!estimate_kbps_) {
        estimate_kbps_ = sample_kbps;
    } else {
        // exponential moving average
        estimate_kbps_ = smoothing_coeff * estimate_kbps_.value() + (1 - smoothing_coeff) * sample_kbps;
    }
    // Calculate the estimated variance of the link capacity estimate
    // norm in the range: [1, estimate_kbps_]
    const double norm = std::max(estimate_kbps_.value(), 1.0);
    double error_kbps = estimate_kbps_.value() - sample_kbps;
    // Normalize the variance with the link capacity estimate.
    double normalized_variance_kbps = (error_kbps * error_kbps /* variance */) / norm;
    variance_kbps_ = smoothing_coeff * variance_kbps_ + (1- smoothing_coeff) * normalized_variance_kbps;
    // normalized_variance = (error_kbps * error_kbps) / norm
    // 0.4 ~= 0.392 = (14 * 14) / 500 = 14 kbit/s at 500 kbit/s
    // 2.5 ~= 2.45 = (35 * 35) / 500 = 35 kbit/s at 500 kbit/s
    // Clamps `variance_kbps_` in the range: [0.4, 2.5]
    variance_kbps_ = std::max<double>(variance_kbps_, 0.4f);
    variance_kbps_ = std::min<double>(variance_kbps_, 2.5f);
}

double LinkCapacityEstimator::EstimatedStdDev() const {
    // Calculate the max bit rate standard deviation given the normalized
    // variance and the current throughout bitrate.
    // The standard deviation will only be used if the `estimate_kbps_` has
    // a value.
    if (estimate_kbps_.has_value()) {
        return sqrt(variance_kbps_ * estimate_kbps_.value());
    } else {
        return 0.0f;
    }
}

} // namespace naivertc