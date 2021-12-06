#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_LINK_CAPACITY_ESTIMATOR_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_LINK_CAPACITY_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/units/data_rate.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT LinkCapacityEstimator {
public:
    LinkCapacityEstimator();
    ~LinkCapacityEstimator();

    void Reset();
    void OnOveruseDetected(DataRate acknowledged_rate);
    void OnProbeRate(DataRate probe_rate);
    std::optional<DataRate> UpperBound() const;
    std::optional<DataRate> LowerBound() const;
    std::optional<DataRate> Estimate() const;
private:
    void Update(DataRate capacity_sample, double alpha);
    double EstimatedStdDev() const;

private:
    std::optional<double> estimate_kbps_;
    double variance_kbps_;
};
    
} // namespace naivertc

#endif
 