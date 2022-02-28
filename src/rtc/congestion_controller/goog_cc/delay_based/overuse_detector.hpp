#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_OVERUSE_DETECTOR_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_OVERUSE_DETECTOR_H_

#include "base/defines.hpp"
#include "rtc/congestion_controller/base/bwe_defines.hpp"

#include <optional>

namespace naivertc {

class OveruseDetector {
public:
    OveruseDetector();
    ~OveruseDetector();

    BandwidthUsage Detect(std::optional<double> trend, 
                          double inter_departure_ms, 
                          size_t num_samples, 
                          int64_t now_ms);

    BandwidthUsage State() const;

private:
    void UpdateThreshold(const double modified_trend, int now_ms);
    
private:
    // Parameters used to update threshold.
    const double k_up_;
    const double k_down_;
    const double threshold_gain_;
    double threshold_;
    int64_t last_update_ms_;

    // Parameters used to detecte bandwidth usage. 
    double last_trend_;
    // The continuous time of being over-using state since the previous sample.
    double overuse_continuous_time_ms_;
    // The accumated count of being over-using state since the prevous sample.
    size_t overuse_accumated_counter_;
    double overuse_time_threshold_;
    size_t overuse_count_threshold_;
    BandwidthUsage bandwidth_usage_;

    DISALLOW_COPY_AND_ASSIGN(OveruseDetector);
};
    
} // namespace naivertc


#endif