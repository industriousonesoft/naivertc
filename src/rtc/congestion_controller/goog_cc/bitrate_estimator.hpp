#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_controller/goog_cc/bitrate_estimator_interface.hpp"

namespace naivertc {

class RTC_CPP_EXPORT BitrateEstimator : public BitrateEstimatorInterface {
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
    ~BitrateEstimator() override;

    void Update(Timestamp at_time, size_t amount, bool in_alr) override;

    std::optional<DataRate> Estimate() const override;
    std::optional<DataRate> PeekRate() const override;

    void ExpectFastRateChange() override;

private:
    // Return a pari consisting of the immediate bitrate in kbps and
    // a bool denoting whether the count of accumulated bytes is small  
    // than `small_sample_threshold` defined configuration.
    std::pair<float, bool> CalcImmediateBitrate(int64_t now_ms,
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