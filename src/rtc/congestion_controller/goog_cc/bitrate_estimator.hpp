#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT BitrateEstimator {
public:
    struct Configuration {
        int initial_window_ms = 500;
        int noninitial_window_ms = 150;
        double uncertainty_scale = 10.0;
        double uncertainty_scale_in_alr = 10.0;
        double small_sample_uncertainty_scale = 10.0;
        size_t small_sample_threshold = 0;
        DataRate uncertainty_symmetry_cap = DataRate::Zero();
        DataRate estimate_floor = DataRate::Zero();
    };
public:
    BitrateEstimator(Configuration config);
    ~BitrateEstimator();

    void Update(Timestamp at_time, size_t amount, bool in_alr);

    std::optional<DataRate> Estimate() const;
    std::optional<DataRate> PeekRate() const;

    void ExpectedFastRateChange();

private:
    std::pair<float, bool> UpdateWindow(int64_t now_ms,
                                        int bytes,
                                        int rate_window_ms);

private:
    const Configuration config_;
    size_t accumulated_bytes_;
    int64_t curr_window_ms_;
    std::optional<int64_t> prev_time_ms_;
    float bitrate_estimate_kbps_;
    float bitrate_estimate_var_;
};
    
} // namespace naivertc


#endif