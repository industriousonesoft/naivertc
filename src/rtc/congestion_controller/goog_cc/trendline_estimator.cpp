#include "rtc/congestion_controller/goog_cc/trendline_estimator.hpp"

#include <plog/Log.h>

#include <algorithm>

namespace naivertc {
namespace {

// Parameters for linear least squares fit of regression line to noisy data.
constexpr double kDefaultTrendlineSmoothingCoeff = 0.9;
constexpr double kDefaultTrendlineThresholdGain = 4.0;
constexpr double kMaxAdaptOffsetMs = 15.0;
constexpr double kOverUsingTimeThresholdMs = 10;
constexpr int kMinNumDeltas = 60;
constexpr int kDeltaCounterMax = 1000;
    
} // namespace

// Analysis and Design of the Google Congestion Control for WebRTC.
// See https://c3lab.poliba.it/images/6/65/Gcc-analysis.pdf
TrendlineEstimator::TrendlineEstimator(Configuration config) 
    : config_(std::move(config)),
      smoothing_coeff_(kDefaultTrendlineSmoothingCoeff),
      threshold_gain_(kDefaultTrendlineThresholdGain),
      num_of_deltas_(0),
      first_arrival_time_ms_(-1),
      accumulated_delay_ms_(0),
      smoothed_delay_ms_(0),
      k_up_(0.0087),
      k_down_(0.039),
      overusing_time_threshold_(kOverUsingTimeThresholdMs),
      threshold_(12.5),
      prev_modified_trend_(NAN),
      last_update_ms_(-1),
      prev_trend_(0.0),
      time_over_using_ms_(-1),
      overuse_counter_(0),
      estimated_state_(BandwidthUsage::NORMAL) {}

TrendlineEstimator::~TrendlineEstimator() = default;

void TrendlineEstimator::Update(double recv_delta_ms,
                                double send_delta_ms,
                                int64_t send_time_ms,
                                int64_t arrival_time_ms,
                                size_t packet_size,
                                bool calculated_deltas) {
    if (calculated_deltas) {
        UpdateTrendline(recv_delta_ms, send_delta_ms, send_time_ms, arrival_time_ms, packet_size);
    }
}

BandwidthUsage TrendlineEstimator::State() const {
    return estimated_state_;
}

// Private methods
void TrendlineEstimator::UpdateTrendline(double recv_delta_ms,
                                         double send_delta_ms,
                                         int64_t send_time_ms,
                                         int64_t arrival_time_ms,
                                         size_t packet_size) {
    const double transport_delay_ms = recv_delta_ms - send_delta_ms;
    ++num_of_deltas_;
    num_of_deltas_ = std::min(num_of_deltas_, kDeltaCounterMax);
    if (first_arrival_time_ms_ == -1) {
        first_arrival_time_ms_ = arrival_time_ms;
    }
    
    accumulated_delay_ms_ += transport_delay_ms;
    // Exponential backoff filter.
    smoothed_delay_ms_ = smoothing_coeff_ * smoothed_delay_ms_ + (1 - smoothing_coeff_) * accumulated_delay_ms_;

    // Maintain packet window.
    delay_hits_.emplace_back(static_cast<double>(arrival_time_ms - first_arrival_time_ms_), smoothed_delay_ms_, accumulated_delay_ms_);
    if (config_.enable_sort) {
        for (size_t i = delay_hits_.size() - 1;
             i > 0 &&
             delay_hits_[i].arrival_time_span_ms < delay_hits_[i - 1].arrival_time_span_ms;
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
    if (delay_hits_.size() == config_.window_size) {
        // Update `trend` if it is possible to fit a line to the data. The delay
        // trend can be seen as an estimate of (send_rate - capacity) / capacity.
        // 0 < trend < 1   ->  the delay increases, queues are filling up
        //   trend == 0    ->  the delay does not change
        //   trend < 0     ->  the delay decreases, queues are being emptied
        trend = CalcLinearFitSlope().value_or(trend);
        if (config_.enable_cap) {
            auto slope_cap = CalcSlopeCap();
            // We only use the cap to filter out overuse detections, not
            // to detect additional underuses.
            if (trend >= 0 && slope_cap.has_value() && trend > slope_cap.value()) {
                trend = slope_cap.value();
            }
        }
    }

    Detect(trend, send_delta_ms, arrival_time_ms);
}

void TrendlineEstimator::Detect(double trend, double ts_delta, int64_t now_ms) {
    if (num_of_deltas_ < 2) {
        estimated_state_ = BandwidthUsage::NORMAL;
        return;
    }
    const double modified_trend = std::min<double>(num_of_deltas_, kMinNumDeltas) * trend * threshold_gain_;
    prev_modified_trend_ = modified_trend;
    if (modified_trend > threshold_) {
        if (time_over_using_ms_ == -1) {
            // Initialize the timer. Assume that we've been
            // over-using half of the time since the previous
            // sample.
            time_over_using_ms_ = ts_delta / 2;
        } else {
            // Increment timer.
            time_over_using_ms_ += ts_delta;
        }
        ++overuse_counter_;
        if (time_over_using_ms_ > overusing_time_threshold_ && overuse_counter_ > 1) {
            if (trend >= prev_trend_) {
                time_over_using_ms_ = 0;
                overuse_counter_ = 0;
                estimated_state_ = BandwidthUsage::OVERUSING;
            }
        }
    } else if (modified_trend < -threshold_) {
        time_over_using_ms_ = -1;
        overuse_counter_ = 0;
        estimated_state_ = BandwidthUsage::UNDERUSING;
    } else {
        time_over_using_ms_ = -1;
        overuse_counter_ = 0;
        estimated_state_ = BandwidthUsage::NORMAL;
    }
    prev_trend_ = trend;
    UpdateThreshold(modified_trend, now_ms);
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

    // NOTE: Why we usd the adaptive threshold instead of static one:
    // The goal of the adaptive threshold is to adapt the sensitivity of the 
    // algorithm (the least square algorithm) to the delay gradient based on 
    // network conditions.
    // For detail, see https://c3lab.poliba.it/images/6/65/Gcc-analysis.pdf (4.2 Adaptive threshold design)
    const double k = modified_trend_abs < threshold_ ? k_down_ : k_up_;
    const int64_t kMaxTimeDeltaMs = 100;
    int64_t time_delta_ms = std::min<double>(now_ms - last_update_ms_, kMaxTimeDeltaMs);
    // The proposed formula: threshold_i = threshold_i-1 + k_i * (|modified_trend_i| - threshold_i-1) * delta_time 
    threshold_ += k * (modified_trend_abs - threshold_) * time_delta_ms;
    // Clamp `threshold_` in [6.f, 600.f]
    threshold_ = std::max<double>(threshold_, 6.f);
    threshold_ = std::min<double>(threshold_, 600.f);
    last_update_ms_ = now_ms;
}

std::optional<double> TrendlineEstimator::CalcLinearFitSlope() const {
    assert(delay_hits_.size() >= 2);
    // Compute the center of mass.
    double sum_x = 0;
    double sum_y = 0;
    for (const auto& pt : delay_hits_) {
        sum_x += pt.arrival_time_span_ms;
        sum_y += pt.smoothed_delay_ms;
    }
    double x_avg = sum_x / delay_hits_.size();
    double y_avg = sum_y / delay_hits_.size();
    // Least square:
    // y = k*x + b
    // error = y_i - y^ = y_i - (k*x_i + b)
    // Compute the slope k = ∑(x_i-x_avg)(y_i-y_avg) / ∑(x_i-x_avg)^2
    double numerator = 0;
    double denominator = 0;
    for (const auto& pt : delay_hits_) {
        double x = pt.arrival_time_span_ms;
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
    auto early = delay_hits_[0];
    for (size_t i = 1; i < config_.beginning_packets; ++i) {
        if (delay_hits_[i].accumulated_delay_ms < early.accumulated_delay_ms) {
            early = delay_hits_[i];
        }
    }
    size_t late_start = delay_hits_.size() - config_.end_packets;
    auto late = delay_hits_[late_start];
    for (size_t i = late_start + 1; i < delay_hits_.size(); ++i) {
        if (delay_hits_[i].accumulated_delay_ms < late.accumulated_delay_ms) {
            late = delay_hits_[i];
        }
    }
    if (late.arrival_time_span_ms - early.arrival_time_span_ms < 1 /* 1ms */) {
        return std::nullopt;
    }
    return (late.accumulated_delay_ms - early.accumulated_delay_ms) / (late.arrival_time_span_ms - early.arrival_time_span_ms) + config_.cap_uncertainty;
}
    
} // namespace naivertc
