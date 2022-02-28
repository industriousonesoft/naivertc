#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"

#include <optional>

namespace naivertc {

// The class used to compute a bayesian estimate of the throughput
// given acks containing the arrival time and acknowledged bytes. 
// 贝叶斯估计是一种常用的基于观测数据作为参数来预估目标参数的算法，类似的算法还有
// MLP（最大似然估计）和MAP（最大后延估计）。
// 贝叶斯估计：假设观测参数服从一种分布，即先验分布。同样目标参数也服从一种分布，
// 即后验分布，换言之，目标参数是后验分布中的一个随机数。因此可基于先验估计和观测
// 数据得出后验分布，
class ThroughputEstimator {
public:
    // Hyperparameter
    struct Hyperparameter {
        int initial_window_ms = 500;
        int noninitial_window_ms = 150;
        double uncertainty_scale = 10.0;
        double uncertainty_scale_in_alr = 20.0;
        double small_sample_uncertainty_scale = 20.0;
        size_t small_sample_threshold = 0;
        DataRate uncertainty_symmetry_cap = DataRate::Zero();
        DataRate estimate_floor = DataRate::Zero();
    };
    using Configuration = Hyperparameter;
public:
    ThroughputEstimator(Configuration config);
    virtual ~ThroughputEstimator();

    virtual void Update(Timestamp at_time, size_t acked_bytes, bool in_alr);

    virtual std::optional<DataRate> Estimate() const;
    virtual std::optional<DataRate> PeekRate() const;

    virtual void ExpectFastRateChange();

private:
    float UpdateWindow(int64_t now_ms,
                       int bytes,
                       const int rate_window_ms,
                       bool* is_small_sample);

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