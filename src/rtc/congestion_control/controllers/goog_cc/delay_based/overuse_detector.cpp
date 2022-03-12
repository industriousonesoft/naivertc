#include "rtc/congestion_control/controllers/goog_cc/delay_based/overuse_detector.hpp"

namespace naivertc {
namespace {

constexpr double kDefaultTrendlineThresholdGain = 4.0;
constexpr size_t kOverUsingCountThreshold = 1;
constexpr double kMaxAdaptOffsetMs = 15.0;
constexpr double kOverUsingTimeThresholdMs = 10;
constexpr size_t kMinNumSamples = 60;
    
} // namespace


OveruseDetector::OveruseDetector() 
    : k_up_(0.0087),
      k_down_(0.039),
      threshold_gain_(kDefaultTrendlineThresholdGain),
      threshold_(12.5),
      last_update_ms_(-1),
      last_trend_(0.0),
      overuse_continuous_time_ms_(-1),
      overuse_accumated_counter_(0),
      overuse_time_threshold_(kOverUsingTimeThresholdMs),
      overuse_count_threshold_(kOverUsingCountThreshold),
      bandwidth_usage_(BandwidthUsage::NORMAL) {}
    
OveruseDetector::~OveruseDetector() {}

BandwidthUsage OveruseDetector::State() const {
    return bandwidth_usage_;
}

BandwidthUsage OveruseDetector::Detect(std::optional<double> new_trend, 
                                       double inter_departure_ms, 
                                       size_t num_samples, 
                                       int64_t now_ms) {
    // Too little samples to do detection.
    if (num_samples < 2) {
        return BandwidthUsage::NORMAL;
    } 

    const double trend = new_trend.value_or(last_trend_);
    // FIXME: How to understand the formula below? It's a low-pass filter?
    const double enhanced_trend = std::min<double>(num_samples, kMinNumSamples) * trend * threshold_gain_;

    // Overusing
    if (enhanced_trend > threshold_) {
        if (overuse_continuous_time_ms_ == -1) {
            // Initialize the timer. Assume that we've been over-using half of 
            // the time since the previous sample.
            overuse_continuous_time_ms_ = inter_departure_ms / 2;
        } else {
            // Increment timer.
            overuse_continuous_time_ms_ += inter_departure_ms;
        }
        ++overuse_accumated_counter_;
        // Not detect sensitively, do update state only 
        // both of the following conditions are true:
        // 1. We've been over-using over 10 ms;
        // 2. It's not the first time we have detected overuse state;
        // 3. The new trend is increasing or not decreasing, we think 
        // the current network state is fine.
        if (overuse_continuous_time_ms_ > overuse_time_threshold_ && 
            overuse_accumated_counter_ > overuse_count_threshold_ &&
            trend >= last_trend_) {
            overuse_continuous_time_ms_ = 0;
            overuse_accumated_counter_ = 0;
            bandwidth_usage_ = BandwidthUsage::OVERUSING;
        } else {
            // Otherwise, we just keep the previous state. 
        }
    // Underusing
    } else if (enhanced_trend < -threshold_) {
        overuse_continuous_time_ms_ = -1;
        overuse_accumated_counter_ = 0;
        bandwidth_usage_ = BandwidthUsage::UNDERUSING;
    // Nomal if enhanced trend in the range [-threshold_, threshold_].
    } else {
        overuse_continuous_time_ms_ = -1;
        overuse_accumated_counter_ = 0;
        bandwidth_usage_ = BandwidthUsage::NORMAL;
    }
#if ENABLE_TEST_DEBUG
    GTEST_COUT << "enhanced_trend: " << enhanced_trend << " vs "
               << "threshold: " << threshold_ << " - "
               << "trend: " << trend << " vs "
               << "last_trend: " << last_trend_ << " - "
               << "estimated_state: " << estimated_state_
               << std::endl;
#endif
    last_trend_ = trend;
    // Adpat the threshold to the trend change.
    UpdateThreshold(enhanced_trend, now_ms);
    return bandwidth_usage_;
}

void OveruseDetector::UpdateThreshold(const double enhanced_trend, int now_ms) {
    if (last_update_ms_ == -1) {
        last_update_ms_ = now_ms;
    }
    double enhanced_trend_abs = fabs(enhanced_trend);
    if (enhanced_trend_abs > threshold_ + kMaxAdaptOffsetMs) {
        // Avoid adapting the threshold to big letency spikes.
        last_update_ms_ = now_ms;
        return;
    }

    // NOTE: The goal of the adaptive threshold is to adapt the sensitivity of the 
    // algorithm (the least square algorithm) to the delay gradient based on 
    // network conditions.
    // The parameters |k_up_| and |k_down_| determine the algorithm sensitivity
    // to the estimated one way delay gradient |modified_trend|.
    // For detail, see https://c3lab.poliba.it/images/6/65/Gcc-analysis.pdf (4.2 Adaptive threshold design)
    const double k = enhanced_trend_abs < threshold_ ? k_down_ : k_up_;
    // FIXME: 此处的|kMaxTimeDeltaMs|取值与InterArrivalDelta类中的|kMaxBurstDuration|相同，二者之间存在联系吗？
    const int64_t kMaxTimeDeltaMs = 100;
    int64_t time_delta_ms = std::min<double>(now_ms - last_update_ms_, kMaxTimeDeltaMs);
    // γ(ti) = γ(ti−1) + ∆T · kγ (ti)(|m(ti)| − γ(ti−1))
    // The proposed formula: threshold_i = threshold_i-1 + k_i * (|modified_trend_i| - threshold_i-1) * delta_time 
    threshold_ += k * (enhanced_trend_abs - threshold_) * time_delta_ms;
    // Clamp |threshold_| in [6.f, 600.f].
    threshold_ = std::max<double>(threshold_, 6.f);
    threshold_ = std::min<double>(threshold_, 600.f);
    last_update_ms_ = now_ms;
}
    
} // namespace naivertc
