#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_estimator.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {
namespace {

static constexpr uint32_t kStartupDelaySamples = 30;
static constexpr int64_t kFsAccuStartupSamples = 5;
static constexpr double kMaxEstimatedFrameRate = 200.0;
static constexpr int64_t kNackCountTimeoutMs = 60000;
static constexpr double kDefaultMaxTimestampDeviationInSigmas = 3.5;
static constexpr double kJumpStdDevForDetectingKeyFrame = 2.0;

}  // namespace

JitterEstimator::JitterEstimator(std::shared_ptr<Clock> clock) 
    : phi_(0.97),
      psi_(0.9999),
      sample_count_max_(400),
      theta_low_(0.000001),
      nack_limit_(3),
      num_std_dev_delay_outlier_(15),
      num_std_dev_frame_size_outlier_(3),
      noise_std_devs_(2.33),
      noise_std_dev_offset_(30.0),
      time_deviation_upper_bound_(kDefaultMaxTimestampDeviationInSigmas),
      enable_reduced_delay_(true),
      // TODO: Use an estimator with limit base on time rather than number of samples.
      frame_delta_us_accumulator_(30 /* 30 us */),
      clock_(std::move(clock)) {}

JitterEstimator::~JitterEstimator() {}

void JitterEstimator::Reset() {
    theta_[0] = 1 / (512e3 / 8);
    theta_[1] = 0;
    var_noise_ = 4.0;
    theta_cov_[0][0] = 1e-4;
    theta_cov_[1][1] = 1e2;
    theta_cov_[0][1] = theta_cov_[1][0] = 0;
    Q_cov_[0][0] = 2.5e-10;
    Q_cov_[1][1] = 1e-10;
    Q_cov_[0][1] = Q_cov_[1][0] = 0;
    avg_frame_size_ = 500;
    max_frame_size_ = 500;
    var_frame_size_ = 100;
    last_update_time_us_ = -1;
    prev_estimate_ = -1.0;
    prev_frame_size_ = 0;
    avg_noise_ = 0.0;
    sample_count_ = 1;
    filtered_sum_of_jitter_estimates_ = 0.0;
    latest_nack_timestamp_ = 0;
    nack_count_ = 0;
    frame_size_sum_ = 0;
    frame_count_ = 0;
    startup_count_ = 0;

    rtt_filter_.Reset();
    frame_delta_us_accumulator_.Reset();
}

void JitterEstimator::UpdateEstimate(int64_t frame_delay_ms, 
                                     uint32_t frame_size, 
                                     bool incomplete_frame) {
    if (frame_size == 0) {
        return;
    }
    int frame_size_delta = frame_size - prev_frame_size_;
    if (frame_count_ < kFsAccuStartupSamples) {
        frame_size_sum_ += frame_size;
        ++frame_count_;
    } else if (frame_count_ == kFsAccuStartupSamples) {
        avg_frame_size_ = static_cast<double>(frame_size_sum_) / static_cast<double>(frame_count_);
        ++frame_count_;
    }

    if (!incomplete_frame || frame_size > avg_frame_size_) {
        // Moving average
        double new_avg_frame_size = phi_ * avg_frame_size_ + (1 - phi_) * frame_size;
        // Only update the average frame size if this frame wasn't a key frame.
        if (frame_size < avg_frame_size_ + kJumpStdDevForDetectingKeyFrame * sqrt(var_frame_size_)) {
            avg_frame_size_ = new_avg_frame_size;
        }
        // Update the variance anyway since we want to capture cases where we only get key frames.
        var_frame_size_ = phi_ * var_frame_size_ + (1 - phi_) * pow(frame_size - new_avg_frame_size, 2);
        if (var_frame_size_ > 1.0) {
            var_frame_size_ = 1.0;
        }
    }

    // Update max frame size estimate.
    max_frame_size_ = std::max(phi_ * max_frame_size_, static_cast<double>(frame_size));

    if (prev_frame_size_ == 0) {
        prev_frame_size_ = frame_size;
        return;
    }
    prev_frame_size_ = frame_size;

    int64_t max_time_deviation_ms = static_cast<int64_t>(time_deviation_upper_bound_ * sqrt(var_noise_) + 0.5);
    // Limits `frame_delay_ms` in the rang [-max_time_deviation_ms, max_time_deviation_ms].
    frame_delay_ms = std::max(std::min(frame_delay_ms, max_time_deviation_ms), -max_time_deviation_ms);

    double delay_deviation = DeviationFromExpectedDelay(frame_delay_ms, frame_size_delta);
    // Only update the Kalman filter:
    // 1). The sample is not considered an extreme outlier,
    // 2). The sample an extreme outlier from a deley point of view, and the frame size also
    //     is large then the deviation.
    // FIXME: How to understand the second condition?
    if (fabs(delay_deviation) < num_std_dev_delay_outlier_ * sqrt(var_noise_) || 
        frame_size > avg_frame_size_ + num_std_dev_frame_size_outlier_ * sqrt(var_frame_size_)) {
        // Update the variance of the deviation from the line given by the Kalman filter.

    }
}

// Private methods
double JitterEstimator::DeviationFromExpectedDelay(int64_t frame_delay_ms, int32_t frame_size_delta) {
    // theta_[0] and theta_[1] is estimated by Kalman filter.
    // Calculate estimated delay based on linear regression.
    double estimated_delay_ms = theta_[0] * frame_size_delta + theta_[1];
    return frame_delay_ms - estimated_delay_ms;
}

void JitterEstimator::EstimateRandomJitter(double d_dT, bool incomplete_frame) {
    uint64_t now_us = clock_->now_us();
    if (last_update_time_us_ != -1) {
        // Delta from last frame.
        frame_delta_us_accumulator_.AddSample(now_us - last_update_time_us_);
    }
    last_update_time_us_ = now_us;

    if (sample_count_ == 0) {
        return;
    }

    double filt_factor = static_cast<double>(sample_count_ - 1) / static_cast<double>(sample_count_);
    ++sample_count_;

    double estimated_fps = EstimatedFrameRate();
    // In order to avoid a low frame rate stream to react slower to change,
    // scale the filt_factor weight relative a 30 fps stream.
    if (estimated_fps > 0.0) {
        double rate_scale = 30.0 / estimated_fps;
        // At startup, there can be a lot of noise in the fps estimate.
        // Interpolate rate_scale linearly, from 1.0 at sample #1, to 30.0 / fps
        // at sample #kStartupDelaySamples.
        if (sample_count_ < kStartupDelaySamples) {
            rate_scale = (sample_count_ * rate_scale + (kStartupDelaySamples - sample_count_)) / kStartupDelaySamples;
        }
        filt_factor = pow(filt_factor, rate_scale);
    }

    double new_avg_noise = filt_factor * avg_noise_ + (1 - filt_factor) * d_dT;
    double new_var_noise = filt_factor * var_noise_ + (1 - filt_factor) * pow(d_dT - avg_noise_, 2);
    if (!incomplete_frame || new_var_noise > var_noise_) {
        avg_noise_ = new_avg_noise;
        var_noise_ = new_var_noise;
    }
    // The variance should never be zero, since we might get stuck 
    // and consider all samples as outliers.
    if (var_noise_ < 1.0) {
        var_noise_ = 1.0;
    }
}

double JitterEstimator::EstimatedFrameRate() const {
    if (frame_delta_us_accumulator_.ComputeMean() == 0) {
        return 0;
    }
    // Estimate the FPS based on mean of accumulated frame deltas.
    double estimated_fps = 1000000.0 / frame_delta_us_accumulator_.ComputeMean();
    assert(estimated_fps >= 0);

    if (estimated_fps > kMaxEstimatedFrameRate) {
        estimated_fps = kMaxEstimatedFrameRate;
    }
    return estimated_fps;
}
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivert 