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
      overusing_time_threshold_ms_(kOverUsingTimeThresholdMs),
      threshold_ms_(12.5),
      prev_modified_trend_ms_(NAN),
      last_update_ms_(-1),
      prev_trend_ms_(0.0),
      time_over_using_ms_(-1),
      overuse_counter_(0),
      hypothesis_(BandwidthUsage::NORMAL),
      hypothesis_predicted_(BandwidthUsage::NORMAL) {}

TrendlineEstimator::~TrendlineEstimator() = default;

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
    packet_timings_.emplace_back(static_cast<double>(arrival_time_ms - first_arrival_time_ms_), smoothed_delay_ms_, accumulated_delay_ms_);
    if (config_.enable_sort) {
        for (size_t i = packet_timings_.size() - 1;
             i > 0 &&
             packet_timings_[i].arrival_time_ms < packet_timings_[i - 1].arrival_time_ms;
             --i) {
            std::swap(packet_timings_[i], packet_timings_[i - 1]);
        }
    }
    // Drop the earliest packet if overflowed.
    if (packet_timings_.size() > config_.window_size) {
        packet_timings_.pop_front();
    }

    // Simple linear regression.
    double trend_ms = prev_trend_ms_;
    if (packet_timings_.size() == config_.window_size) {
        // Update `trend_ms` if it is possible to fit a line to the data. The delay
        // trend can be seen as an estimate of (send_rate - capacity) / capacity.
        // 0 < trend < 1   ->  the delay increases, queues are filling up
        //   trend == 0    ->  the delay does not change
        //   trend < 0     ->  the delay decreases, queues are being emptied
        trend_ms = CalcLinearFitSlope().value_or(trend_ms);
        if (config_.enable_cap) {
            auto slope_cap = CalcSlopeCap();
            // We only use the cap to filter out overuse detections, not
            // to detect additional underuses.
            if (trend_ms >= 0 && slope_cap.has_value() && trend_ms > slope_cap.value()) {
                trend_ms = slope_cap.value();
            }
        }
    }

    Detect(trend_ms, send_delta_ms, arrival_time_ms);
}

void TrendlineEstimator::Detect(double trend_ms, double ts_delta, int64_t now_ms) {
    if (num_of_deltas_ < 2) {
        hypothesis_ = BandwidthUsage::NORMAL;
        return;
    }
    const double modified_trend = std::min<double>(num_of_deltas_, kMinNumDeltas) * trend_ms * threshold_gain_;
    prev_modified_trend_ms_ = modified_trend;
    if (modified_trend > threshold_ms_) {
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
        if (time_over_using_ms_ > overusing_time_threshold_ms_ && overuse_counter_ > 1) {
            if (trend_ms >= prev_trend_ms_) {
                time_over_using_ms_ = 0;
                overuse_counter_ = 0;
                hypothesis_ = BandwidthUsage::OVERUSING;
            }
        }
    } else if (modified_trend < -threshold_ms_) {
        time_over_using_ms_ = -1;
        overuse_counter_ = 0;
        hypothesis_ = BandwidthUsage::UNDERUSING;
    } else {
        time_over_using_ms_ = -1;
        overuse_counter_ = 0;
        hypothesis_ = BandwidthUsage::NORMAL;
    }
    prev_trend_ms_ = trend_ms;
    UpdateThreshold(modified_trend, now_ms);
}

void TrendlineEstimator::UpdateThreshold(double modified_trend_ms, int now_ms) {
    if (last_update_ms_ == -1) {
        last_update_ms_ = now_ms;
    }
    double modified_trend_asb = fabs(modified_trend_ms);
    if (modified_trend_asb > threshold_ms_ + kMaxAdaptOffsetMs) {
        // Avoid adapting the threshold to big letency spikes.
        last_update_ms_ = now_ms;
        return;
    }

    const double k = modified_trend_asb < threshold_ms_ ? k_down_ : k_up_;
    const int64_t kMaxTimeDeltaMs = 100;
    int64_t time_delta_ms = std::min<double>(now_ms - last_update_ms_, kMaxTimeDeltaMs);
    threshold_ms_ += k * (modified_trend_asb - threshold_ms_) * time_delta_ms;
    threshold_ms_ = std::min<double>(threshold_ms_, 6.f);
    threshold_ms_ = std::max<double>(threshold_ms_, 600.f);
    last_update_ms_ = now_ms;
}

std::optional<double> TrendlineEstimator::CalcLinearFitSlope() const {
    assert(packet_timings_.size() >= 2);
    // Compute the center of mass.
    double sum_x = 0;
    double sum_y = 0;
    for (const auto& pt : packet_timings_) {
        sum_x += pt.arrival_time_ms;
        sum_y += pt.smoothed_delay_ms;
    }
    double x_avg = sum_x / packet_timings_.size();
    double y_avg = sum_y / packet_timings_.size();
    // Compute the slope k = ∑(x_i-x_avg)(y_i-y_avg) / ∑(x_i-x_avg)^2
    double numerator = 0;
    double denominator = 0;
    for (const auto& pt : packet_timings_) {
        double x = pt.arrival_time_ms;
        double y = pt.smoothed_delay_ms;
        numerator += (x - x_avg) * (y - y_avg);
        denominator += pow(x - x_avg, 2);
    }
    return denominator == 0 ? std::nullopt : std::optional<double>(numerator / denominator);
}

std::optional<double> TrendlineEstimator::CalcSlopeCap() const {
    assert(config_.beginning_packets >= 1 && config_.beginning_packets < packet_timings_.size());
    assert(config_.end_packets >= 1 && config_.end_packets < packet_timings_.size());
    assert(config_.beginning_packets + config_.end_packets <= packet_timings_.size());
    auto early = packet_timings_[0];
    for (size_t i = 1; i < config_.beginning_packets; ++i) {
        if (packet_timings_[i].accumulated_delay_ms < early.accumulated_delay_ms) {
            early = packet_timings_[i];
        }
    }
    size_t late_start = packet_timings_.size() - config_.end_packets;
    auto late = packet_timings_[late_start];
    for (size_t i = late_start + 1; i < packet_timings_.size(); ++i) {
        if (packet_timings_[i].accumulated_delay_ms < late.accumulated_delay_ms) {
            late = packet_timings_[i];
        }
    }
    if (late.arrival_time_ms - early.arrival_time_ms < 1 /* 1ms */) {
        return std::nullopt;
    }
    return (late.accumulated_delay_ms - early.accumulated_delay_ms) / (late.arrival_time_ms - early.arrival_time_ms) + config_.cap_uncertainty;
}
    
} // namespace naivertc
