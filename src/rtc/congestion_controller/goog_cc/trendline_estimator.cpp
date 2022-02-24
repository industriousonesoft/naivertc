#include "rtc/congestion_controller/goog_cc/trendline_estimator.hpp"

#include <plog/Log.h>

#include <algorithm>

#define ENABLE_TEST_DEBUG (ENABLE_TESTS && 0)
#if ENABLE_TEST_DEBUG
#include "testing/defines.hpp"
#endif

namespace naivertc {
namespace {

// Parameters for linear least squares fit of regression line to noisy data.
constexpr double kDefaultTrendlineSmoothingCoeff = 0.9;
constexpr double kDefaultTrendlineThresholdGain = 4.0;
constexpr double kMaxAdaptOffsetMs = 15.0;
constexpr double kOverUsingTimeThresholdMs = 10;
constexpr int kOverUsingCountThreshold = 1;
constexpr int kMinNumSamples = 60;
constexpr int kMmaxNumSamples = 1000;
    
} // namespace

// Analysis and Design of the Google Congestion Control for WebRTC.
// See https://c3lab.poliba.it/images/6/65/Gcc-analysis.pdf
TrendlineEstimator::TrendlineEstimator(Configuration config) 
    : config_(std::move(config)),
      smoothing_coeff_(kDefaultTrendlineSmoothingCoeff),
      threshold_gain_(kDefaultTrendlineThresholdGain),
      num_of_samples_(0),
      first_arrival_time_ms_(-1),
      accumulated_delay_ms_(0),
      smoothed_delay_ms_(0),
      k_up_(0.0087),
      k_down_(0.039),
      overusing_count_threshold_(kOverUsingCountThreshold),
      overusing_time_threshold_(kOverUsingTimeThresholdMs),
      threshold_(12.5),
      prev_modified_trend_(NAN),
      last_update_ms_(-1),
      prev_trend_(0.0),
      overuse_continuous_time_ms_(-1),
      overuse_accumated_counter_(0),
      estimated_state_(BandwidthUsage::NORMAL) {}

TrendlineEstimator::~TrendlineEstimator() = default;

BandwidthUsage TrendlineEstimator::Update(double recv_delta_ms,
                                          double send_delta_ms,
                                          int64_t send_time_ms,
                                          int64_t arrival_time_ms,
                                          size_t packet_size) {
    return UpdateTrendline(recv_delta_ms, 
                           send_delta_ms, 
                           send_time_ms, 
                           arrival_time_ms, 
                           packet_size);
}

BandwidthUsage TrendlineEstimator::State() const {
    return estimated_state_;
}

// Private methods
BandwidthUsage TrendlineEstimator::UpdateTrendline(double recv_delta_ms,
                                                   double send_delta_ms,
                                                   int64_t send_time_ms,
                                                   int64_t arrival_time_ms,
                                                   size_t packet_size) {
    // Inter-group delay variation between two adjacent groups.
    //    |             |
    // s1 + _           |
    //    |  \ _ _ _    |
    //    |         \ _ + r1
    // s2 + _           |
    //    |  \ _ _ _    |
    //    |   \     \ _ + r2'(expected)
    //    |    \ _ _    |
    //    |         \ _ + r2 (real)
    //    |             |
    // send_delta = s2 - s1
    // recv_delta = r2 - r1
    // propagation_delta = r2' - r2 = recv_delta - send_delta
    const double propagation_delta_ms = recv_delta_ms - send_delta_ms;
    ++num_of_samples_;
    num_of_samples_ = std::min(num_of_samples_, kMmaxNumSamples);
    if (first_arrival_time_ms_ == -1) {
        first_arrival_time_ms_ = arrival_time_ms;
    }
    
    // Accumulate propagation delay.
    accumulated_delay_ms_ += propagation_delta_ms;
    // Exponential backoff filter.
    // Calculate the smoothed accumulated delay.
    smoothed_delay_ms_ = smoothing_coeff_ * smoothed_delay_ms_ + (1 - smoothing_coeff_) * accumulated_delay_ms_;

    // Maintain packet window.
    delay_hits_.emplace_back(/*arrival_time_ms=*/static_cast<double>(arrival_time_ms - first_arrival_time_ms_), 
                             smoothed_delay_ms_, 
                             accumulated_delay_ms_);
    // Sort |delay_hits_| if required. 
    if (config_.enable_sort) {
        // The |delay_hits_| is ordered before emplacing back the new element,
        // so we just need to put the back element to the right postion.
        for (size_t i = delay_hits_.size() - 1;
             i > 0 &&
             delay_hits_[i].arrival_time_ms < delay_hits_[i - 1].arrival_time_ms;
             --i) {
                 std::swap(delay_hits_[i], delay_hits_[i - 1]);
        }
    }
    // Drop the earliest packet if overflowed.
    if (delay_hits_.size() > config_.window_size) {
        delay_hits_.pop_front();
    }

    // Simple linear regression.
    double trend = prev_trend_;
    // We have enough smaples to estmate the trend.
    if (delay_hits_.size() == config_.window_size) {
        // Update `trend` if it is possible to fit a line to the data. The delay
        // trend can be seen as an estimate of (send_rate - capacity) / capacity.
        // 0 < trend < 1   ->  the delay increases, queues are filling up
        //   trend == 0    ->  the delay does not change
        //   trend < 0     ->  the delay decreases, queues are being emptied
        trend = CalcLinearFitSlope(delay_hits_).value_or(trend);
        if (config_.enable_cap) {
            auto slope_cap = CalcSlopeCap();
            // We only use the cap to filter out overuse detections, not
            // to detect additional underuses.
            if (trend >= 0 && slope_cap.has_value() && trend > slope_cap.value()) {
                trend = slope_cap.value();
            }
        }
    }

    // FIXME: The reason of using inter-departure instead of inter-arrval is that we  
    // used the inter-departure to detect the packet group (AKA sample here) in
    // `InterArrivalDelta`?
    return Detect(trend, send_delta_ms, arrival_time_ms);
}

BandwidthUsage TrendlineEstimator::Detect(double trend, double inter_departure_ms, int64_t now_ms) {
    if (num_of_samples_ < 2) {
        estimated_state_ = BandwidthUsage::NORMAL;
        return estimated_state_;
    }
  
    // FIXME: How to understand the formula below?
    const double modified_trend = std::min<double>(num_of_samples_, kMinNumSamples) * trend * threshold_gain_;
    prev_modified_trend_ = modified_trend;

    // Overusing
    if (modified_trend > threshold_) {
        if (overuse_continuous_time_ms_ == -1) {
            // Assume that we've been over-using half of 
            // the time since the previous sample.
            overuse_continuous_time_ms_ = inter_departure_ms / 2;
        } else {
            // Increment timer.
            overuse_continuous_time_ms_ += inter_departure_ms;
        }
        ++overuse_accumated_counter_;
        // No detect overusing sensitively.
        if (overuse_continuous_time_ms_ > overusing_time_threshold_ && 
            overuse_accumated_counter_ > overusing_count_threshold_ &&
            trend >= prev_trend_) {
            overuse_continuous_time_ms_ = 0;
            overuse_accumated_counter_ = 0;
            estimated_state_ = BandwidthUsage::OVERUSING;
        } else {
            // Remains the previous state. 
        }
    // Underusing
    } else if (modified_trend < -threshold_) {
        overuse_continuous_time_ms_ = -1;
        overuse_accumated_counter_ = 0;
        estimated_state_ = BandwidthUsage::UNDERUSING;
    // Nomal
    } else {
        overuse_continuous_time_ms_ = -1;
        overuse_accumated_counter_ = 0;
        estimated_state_ = BandwidthUsage::NORMAL;
    }
#if ENABLE_TEST_DEBUG
    GTEST_COUT << "modified_trend: " << modified_trend << " vs "
               << "threshold: " << threshold_ << " - "
               << "trend: " << trend << " vs "
               << "prev_trend: " << prev_trend_ << " - "
               << "estimated_state: " << estimated_state_
               << std::endl;
#endif
    prev_trend_ = trend;
    UpdateThreshold(modified_trend, now_ms);
    return estimated_state_;
}

void TrendlineEstimator::UpdateThreshold(double modified_trend, int now_ms) {
    if (last_update_ms_ == -1) {
        last_update_ms_ = now_ms;
    }
    double modified_trend_abs = fabs(modified_trend);
    if (modified_trend_abs > threshold_ + kMaxAdaptOffsetMs) {
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
    const double k = modified_trend_abs < threshold_ ? k_down_ : k_up_;
    const int64_t kMaxTimeDeltaMs = 100;
    int64_t time_delta_ms = std::min<double>(now_ms - last_update_ms_, kMaxTimeDeltaMs);
    // γ(ti) = γ(ti−1) + ∆T · kγ (ti)(|m(ti)| − γ(ti−1))
    // The proposed formula: threshold_i = threshold_i-1 + k_i * (|modified_trend_i| - threshold_i-1) * delta_time 
    threshold_ += k * (modified_trend_abs - threshold_) * time_delta_ms;
    // Clamp `threshold_` in [6.f, 600.f].
    threshold_ = std::max<double>(threshold_, 6.f);
    threshold_ = std::min<double>(threshold_, 600.f);
    last_update_ms_ = now_ms;
}

std::optional<double> TrendlineEstimator::CalcLinearFitSlope(const std::deque<PacketTiming>& samples) const {
    assert(samples.size() >= 2);
    // Compute the center of mass.
    double sum_x = 0;
    double sum_y = 0;
    for (const auto& pt : samples) {
        sum_x += pt.arrival_time_ms;
        sum_y += pt.smoothed_delay_ms;
    }
    double x_avg = sum_x / samples.size();
    double y_avg = sum_y / samples.size();
    // 采用最小二乘法（Least Square）:
    // y = k*x + b
    // propagation_delta = k * arrive_time + b
    // error = y_i - y^ = y_i - (k*x_i + b)
    // Compute the slope k = ∑(x_i-x_avg)(y_i-y_avg) / ∑(x_i-x_avg)^2
    // see https://developer.aliyun.com/article/781509
    double numerator = 0;
    double denominator = 0;
    for (const auto& pt : samples) {
        double x = pt.arrival_time_ms;
        double y = pt.smoothed_delay_ms;
        numerator += (x - x_avg) * (y - y_avg);
        denominator += pow(x - x_avg, 2);
    }
    return denominator == 0 ? std::nullopt : std::optional<double>(numerator / denominator);
}

std::optional<double> TrendlineEstimator::CalcSlopeCap() const {
    assert(config_.beginning_packets >= 1 && config_.beginning_packets < delay_hits_.size());
    assert(config_.end_packets >= 1 && config_.end_packets < delay_hits_.size());
    assert(config_.beginning_packets + config_.end_packets <= delay_hits_.size());

    // Find the earlist packet in the begining period.
    auto early = delay_hits_[0];
    for (size_t i = 1; i < config_.beginning_packets; ++i) {
        if (delay_hits_[i].accumulated_delay_ms < early.accumulated_delay_ms) {
            early = delay_hits_[i];
        }
    }
    // Find the earlist packet in the end period.
    size_t late_start = delay_hits_.size() - config_.end_packets;
    auto late = delay_hits_[late_start];
    for (size_t i = late_start + 1; i < delay_hits_.size(); ++i) {
        if (delay_hits_[i].accumulated_delay_ms < late.accumulated_delay_ms) {
            late = delay_hits_[i];
        }
    }
    // Too short to calculate slope (There might has a spike happenned).
    if (late.arrival_time_ms - early.arrival_time_ms < 1 /* 1ms */) {
        return std::nullopt;
    }
    // Calculate slope cap.
    return (late.accumulated_delay_ms - early.accumulated_delay_ms) / (late.arrival_time_ms - early.arrival_time_ms) + config_.cap_uncertainty;
}
    
} // namespace naivertc
