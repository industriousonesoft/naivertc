#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_LINK_CAPACITY_ESTIMATOR_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_LINK_CAPACITY_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/units/data_rate.hpp"

#include <optional>

namespace naivertc {

// Helper class to estimate an average of incoming bitrates.
// It measure this average and standard deviation with an exponential
// moving average with the smoothing factor 0.95.
class LinkCapacityEstimator {
public:
    LinkCapacityEstimator();
    ~LinkCapacityEstimator();

    void Reset();
    void OnOveruseDetected(DataRate acknowledged_rate);
    void OnProbeRate(DataRate probe_rate);
    std::optional<DataRate> UpperBound() const;
    std::optional<DataRate> LowerBound() const;
    // Return estimated average bitrate.
    std::optional<DataRate> Estimate() const;
private:
    void Update(DataRate capacity_sample, double smoothing_coeff);
    double EstimatedStdDev() const;

private:
    std::optional<double> estimate_kbps_;
    double variance_kbps_;
};
    
} // namespace naivertc

#endif
 