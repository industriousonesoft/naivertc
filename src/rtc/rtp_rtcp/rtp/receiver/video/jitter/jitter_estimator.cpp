#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_estimator.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {
namespace {

static constexpr uint32_t kStartupDelaySamples = 30;
static constexpr int64_t kFsAccuStartupSamples = 5;
static constexpr double kMaxEstimatedFrameRate = 200.0;
static constexpr int64_t kNackCountTimeoutUs = 60'000'000; // 60s; 1 minute
static constexpr double kDefaultMaxTimestampDeviationInSigmas = 3.5;
static constexpr double kJumpStdDevForDetectingKeyFrame = 2.0;

}  // namespace

JitterEstimator::JitterEstimator(std::shared_ptr<Clock> clock) 
    : phi_(0.97),
      psi_(0.9999),
      sample_count_max_(400),
      theta_lower_bound_(0.000001),
      nack_limit_(3),
      num_std_dev_delay_outlier_(15),
      num_std_dev_frame_size_outlier_(3),
      noise_std_devs_(2.33),  // Less than 1% change (loop up in normal distribution table)...
      noise_std_dev_offset_(30.0), // ...of getting 30 ms freeses.
      time_deviation_upper_bound_(kDefaultMaxTimestampDeviationInSigmas),
      // TODO: Use an estimator with limit base on time rather than number of samples.
      frame_delta_us_accumulator_(30 /* 30 us */),
      clock_(std::move(clock)) {
    // Reset to the intial values.
    Reset();
}

JitterEstimator::~JitterEstimator() {}

void JitterEstimator::Reset() {
    theta_[0] = 1 / (512e3 / 8);
    theta_[1] = 0;
    var_noise_ = 4.0;
    theta_cov_[0][0] = 1e-4;
    theta_cov_[1][1] = 1e2;
    theta_cov_[0][1] = theta_cov_[1][0] = 0;
    // Q_cov_是一个符合高斯分布的白噪音对角矩阵（对角矩阵：除对角线外的值均为0）
    Q_cov_[0][0] = 2.5e-10;
    Q_cov_[1][1] = 1e-10;
    Q_cov_[0][1] = Q_cov_[1][0] = 0;
    avg_frame_size_ = 500;
    max_frame_size_ = 500;
    var_frame_size_ = 100;
    last_update_time_us_ = -1;
    prev_estimated_jitter_ms_ = -1.0;
    prev_frame_size_ = 0;
    avg_noise_ = 0.0;
    sample_count_ = 1;
    filtered_sum_of_jitter_estimates_ms_ = 0.0;
    latest_nack_time_us_ = 0;
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
        // Weighted Moving Average, `phi_` is a fixed value.
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
    max_frame_size_ = std::max(psi_ * max_frame_size_, static_cast<double>(frame_size));

    if (prev_frame_size_ == 0) {
        prev_frame_size_ = frame_size;
        return;
    }
    prev_frame_size_ = frame_size;

    // The standard deviation of noise.
    double std_dev_noise = sqrt(var_noise_);
    // Cap `frame_delay_ms` based on the current time deviation noise.
    int64_t max_time_deviation_ms = static_cast<int64_t>(time_deviation_upper_bound_ * std_dev_noise + 0.5);
    // Limits `frame_delay_ms` in the rang [-max_time_deviation_ms, max_time_deviation_ms].
    frame_delay_ms = std::max(std::min(frame_delay_ms, max_time_deviation_ms), -max_time_deviation_ms);

    double estimated_delay_deviation = DeviationFromExpectedDelay(frame_delay_ms, frame_size_delta);
    // Only update the Kalman filter:
    // 1). The sample is not considered an extreme outlier,
    // 2). The sample an extreme outlier from a deley point of view, and the frame size also
    //     is large then the deviation.
    // FIXME: How to understand the second condition?
    
    if (fabs(estimated_delay_deviation) < num_std_dev_delay_outlier_ * std_dev_noise || 
        frame_size > avg_frame_size_ + num_std_dev_frame_size_outlier_ * sqrt(var_frame_size_)) {
        // Update the variance of the deviation from the line given by the Kalman filter.
        EstimateRandomJitter(estimated_delay_deviation, incomplete_frame);

        // Prevent updating with frames which have been congested by a large frame,
        // and therefore arrives almost at the same time as that frame.
        // This can occur when we receive a large frame (key frame), and thus `frame_size_delta`
        // will be a negetive number . This removes all frame samples which arrives after a key frame.
        if ((!incomplete_frame || estimated_delay_deviation >= 0.0) &&
            static_cast<double>(frame_size_delta) > -0.25 * max_frame_size_) {
            // Update the Kalman filter with the new data.
            KalmanEstimateChannel(frame_delay_ms, frame_size_delta);
        }
    } else {
        int num_std_dev = estimated_delay_deviation > 0 ? num_std_dev_delay_outlier_ : -num_std_dev_delay_outlier_;
        double delay_deviation_outlier = num_std_dev * std_dev_noise;
        EstimateRandomJitter(delay_deviation_outlier, incomplete_frame);
    }

    // Post process the total estimated jitter.
    if (startup_count_ >= kStartupDelaySamples) {
        filtered_sum_of_jitter_estimates_ms_ = CalcJitterEstimate();
        prev_estimated_jitter_ms_ = filtered_sum_of_jitter_estimates_ms_;
    } else {
        ++startup_count_;
    }
}

int JitterEstimator::GetJitterEstimate(double rtt_multiplier,
                                       std::optional<double> rtt_mult_add_cap_ms,
                                       bool enable_reduced_delay) {
    // The current jitter = estimated jitter + operating system jitter.
    double curr_jitter_ms = CalcJitterEstimate() + kOperatingSystemJitterMs;    
    // Update the previous jitter 
    prev_estimated_jitter_ms_ = curr_jitter_ms;

    // If there is no nack happened during the past one minute, we reset the `nack_count_`.
    int64_t now_us = clock_->now_us();
    if (now_us - latest_nack_time_us_ > kNackCountTimeoutUs /* 60s */) {
        nack_count_ = 0;
    }

    // `filtered_sum_of_jitter_estimates_ms_` has a high prioirty as it's more accurate.
    if (filtered_sum_of_jitter_estimates_ms_ > curr_jitter_ms) {
        curr_jitter_ms = filtered_sum_of_jitter_estimates_ms_;
    }

    // FIXME: How to understan this condition?
    // If there is more than 3 nack happened so far, we need to take the rtt into account.
    if (nack_count_ >= nack_limit_) {
        if (rtt_mult_add_cap_ms.has_value()) {
            curr_jitter_ms += std::min(rtt_filter_.RttMs() * rtt_multiplier, rtt_mult_add_cap_ms.value());
        } else {
            curr_jitter_ms += rtt_filter_.RttMs() * rtt_multiplier;
        }
    }

    if (enable_reduced_delay) {
        static const double kJitterScaleLowThreshold = 5.0;
        static const double kJitterScaleHighThreshold = 10.0;

        double estimated_fps = EstimatedFrameRate();
        // Ignore jitter for very low fps streams.
        if (estimated_fps < kJitterScaleLowThreshold) {
            // FIXME: Why we need to do a different process when the estimated frame rate is zero?
            // DONE: the zero frame rate means there is no frames received so far, and the jitter
            // is estimated based on the initial values.
            if (estimated_fps == 0.0) {
                return static_cast<int>(std::max(0.0, curr_jitter_ms) + 0.5);         
            }
            return 0.0;
        }

        // If the `estimated_fps` is in the range: [kJitterScaleLowThreshold, kJitterScaleHighThreshold), 
        // we assume it's a Semi-low frame rate. 
        // Scaling the Semi-low frame rate by factor linearly interpolated from 0.0
        // at kJitterScaleLowThreshold to 1.0 at kJitterScaleHighThreshold.
        // FIXME: What's the theory about this scale operation?
        if (estimated_fps < kJitterScaleHighThreshold) {
            // scale_factor: [0.0, 1.0) => [kJitterScaleLowThreshold, kJitterScaleHighThreshold)
            double scale_factor = 1.0 * (estimated_fps - kJitterScaleLowThreshold) / (kJitterScaleHighThreshold - kJitterScaleLowThreshold);
            curr_jitter_ms = scale_factor * curr_jitter_ms;
        }
    }

    return static_cast<int>(std::max(0.0, curr_jitter_ms) + 0.5 /* Round up: to increase an figure to the next highest whole number */);                    
}

void JitterEstimator::UpdateRtt(int64_t rtt_ms) {
    rtt_filter_.AddRtt(rtt_ms);
}

void JitterEstimator::FrameNacked() {
    ++nack_count_;
    latest_nack_time_us_ = clock_->now_us();
}

// Private methods
double JitterEstimator::DeviationFromExpectedDelay(int64_t frame_delay_ms, int32_t frame_size_delta) {
    // theta_[0] and theta_[1] is estimated by Kalman filter.
    // Calculate estimated delay based on linear regression.
    // FIXME: Do we consider the `estimated_delay_ms` as the mean of delay?
    double estimated_delay_ms = theta_[0] * frame_size_delta + theta_[1];
    return frame_delay_ms - estimated_delay_ms;
}

void JitterEstimator::EstimateRandomJitter(double d_dT, bool incomplete_frame) {
    uint64_t now_us = clock_->now_us();
    if (last_update_time_us_ != -1) {
        // Record the time in us elapsed since the last frame, and it will be 
        // used to estimate the frame rate later.
        frame_delta_us_accumulator_.AddSample(now_us - last_update_time_us_);
    }
    last_update_time_us_ = now_us;

    // the initial value of `sample_count_` is 1.
    if (sample_count_ == 0) {
        assert(false && "the sample count is never gonna be zero.");
        return;
    }

    // The factor for Moving Average, range: [0, 1), and it's initiated linearly.
    double filt_factor = static_cast<double>(sample_count_ - 1) / static_cast<double>(sample_count_);
    ++sample_count_;
    // FIXME: Why we need to limit `sample_count_`?
    if (sample_count_ > sample_count_max_) {
        sample_count_ = sample_count_max_;
    }

    double estimated_fps = EstimatedFrameRate();
    // In order to avoid a low frame rate stream changing the `filt_factor` slowly,
    // scaling the `filt_factor` exponentially relative a 30 fps stream.
    if (estimated_fps > 0.0) {
        double rate_scale = 30.0 / estimated_fps;
        // At startup, there can be a lot of noise in the fps estimate.
        // Interpolate rate_scale linearly, from 1.0 at sample #1, to 30.0 / fps
        // at sample #kStartupDelaySamples.
        if (sample_count_ < kStartupDelaySamples) {
            // rate_scale = (scaled_samples + rest_of_startup_delay_samples) / startup_delay_samples
            // Reduce the `rate_scale` effect on `filt_factor` at the startup period, since the fps was estimated in low accuracy so far.
            rate_scale = ((sample_count_ * rate_scale) + (kStartupDelaySamples - sample_count_)) / kStartupDelaySamples;
        }
        // FIXME: Why do we scale the `filt_factor` exponentially, not linearly?
        filt_factor = pow(filt_factor, rate_scale);
    }

    // 1 level Exponential Moving Average
    // d_dT: the delay deviation
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
    if (frame_delta_us_accumulator_.ComputeMean() <= 0.0) {
        return 0;
    }
    // Estimate the FPS based on mean of accumulated frame deltas.
    double estimated_fps = 1000000.0 / frame_delta_us_accumulator_.ComputeMean();
    assert(estimated_fps > 0);

    if (estimated_fps > kMaxEstimatedFrameRate) {
        estimated_fps = kMaxEstimatedFrameRate;
    }
    return estimated_fps;
}

void JitterEstimator::KalmanEstimateChannel(int64_t frame_delay_ms, int32_t frame_size_delta) {

    // See https://datatracker.ietf.org/doc/html/draft-alvestrand-rmcat-congestion-03
    // 观测方程建模:
    // 帧间延时计算方式1：
    // 公式1.1：ts(i) = t(i) - T(i)
    // 解析：ts表示测量出来的第i帧的传输时长。
    // 公式1.2：d(i) = t(i) - t(i-1) - (T(i) - T(i-1)) = (t(i) - T(i)) - (t(i-1) - T(i-1))
    // 解析：d(i)表示第i帧与第i-1帧到达接收端用时的差值，即传输延时，t(i)表示第i帧到时间点，T(i)表示第i帧发送时间点。
    // 帧间延时计算方式2：
    // 公式2.1：ts(i) = L(i)/C(i) ~= t(i) - T(i)
    // 解析：ts(i)表示计算的第i帧的传输时长，L(i)表示第i帧所有包的大小总和，C(i)表示当前通道的传输第i帧时的速率。
    // 公式2.2.1：d(i) = L(i)/C(i) - L(i-1)/C(i-1) + w(i)
    // 解析：w(i)表示在传输第i帧时出现的各种随机因素，比如当时的发送码率、通道传输速率、网络拥塞情况等，
    // 且假定w是一个高斯白噪音模型。当通道过载时，w(i)的期望值（mean)呈上升趋势，当通道空载时，w(i)的期望值呈下降趋势，其他情况的期望值为0.
    // 公式2.2.2:         
    //                   L(i)-L(i-1)
    //          d(i) = -------------- + w(i) = dL(i)/C(i) + w(i)
    //                     C(i)
    // 解析：C(i)表示第i帧的传输速率，C(i)的帧间差值相较于L(i)小很多，因此可以假设 C(i) = C(i-1)，进而从公式2.2.1推导出公式2.2.2.
    // 公式2.3: d(i) = dL(i)/C(i) + m(i) + v(i)
    // 解析：将w(i)换另外一种表达方式：w(i) = m(i) + v(i)，其中m(i)表示网络排队延时，当通道超载时均值呈增长趋势，当通道空载时均值呈下降趋势，
    // 其他情况均值为0。v(i)表示测量噪音，属于高斯白噪音，比如dL(i)（使用滑动平均计算的帧相关数据）中的计算误差。
    // 至此，我们得到了我们的观测方程：d(i) = dL(i)/C(i) + m(i) + v(i)，其中d(i)和dL(i)可以通过计算获得，通过估算C(i)和m(i)来检测传输是否过载。
    // 任何具有自适应的滤波器都可以用于估算这两个参数，比如Kalman filter. 以下使用卡尔曼滤波器建模：

    // 状态方程建模：
    // 矩阵1：theta_bar(i) = [1/C(i), m(i)]^T
    // 解析：theta_bar(i)表示第i帧的状态：包括当时的传输速率C(i)和排队延时m(i)。
    // 矩阵2：h_bar(i) = [dL(i), 1]^T
    // 状态方程：theta_bar(i) = theta_bar(i-1) + u_bar(i-1), 
    // 解析：用于表示某一帧传输时的网络状态，其中u_bar(i)表示过程噪音，是一个高斯白噪音矩阵，P(u)~(0,Q)，期望值为0，协方差矩阵为Q.
    // 带入观测方程可得：
    // d(i) = dL(i)/C(i) + m(i) + v(i) 
    //      = [dL(i), 1] * [1/C(i), m(i)]^T + v(i) 
    //      = h_bar(i)^T * theta_bar(i) + v(i)

    // 结合Kalman filter中公式：
    // 状态方程：x(i) = A*x(i-1) + B*u(i-1) + w(i-1) 
    // 解析：x(i)表示某一个状态值，可由前一个状态值x(i-1)乘以系数A，加上促使状态从i-1到i的输入B*u(i-1)，再加上过程转变过程中的噪音w(i-1)，
    // 比如：x(i)表示车子的位置状态，x(i-1)则是上一个位置，系数A表示加速度，匀速则为1。B*u(i-1)表示给一个推动的助力，w(i-1)则表示运动过程中的
    // 摩擦力以及风力带来的助力或阻力。
    // 推导：我们可以把数据在网络中的传输类比传送带传输，即不需要外力就会发生状态变化，因此B*u(i-1)恒等于0，可推导出满足Kalman filer为：
    // theta_bar(i) = theta_bar(i-1) + u_bar(i-1)

    // TODO: 弄清楚具体的推导过程？
    // 结合以上方程和卡尔曼的五个公式（https://blog.csdn.net/wccsu1994/article/details/84643221）,推导出WebRTC中的公式为：
    // 先验估计值：theta^-(i) = theta^(i-1)
    // 先验估计值的协方差：theta_cov-(i) = theta_cov-(i-1) + Q(i)，其中Q(i) = E{u_bar(i) * u_bar(i)^T}，表示过程噪音u_bar(i)的协方差矩阵(对角矩阵)，由于过程噪音无法测量和量化，故一般使用固定值, 
    // 卡尔曼增益：K = theta_cov-(i)*H^T / H*theta_cov-(i)*H^T+R，其中R表示测量噪声协方差。滤波器实际实现时，一般可以观测得到，属于已知条件。
    // 后验估计值：theta^(i) = theta-(k) + K*(d(i)-H*theta^-(i))
    // 后验估计值的协方差：theta_cov(i)=(1 - K*H)*theta_cov-(i)

    // M: 协方差矩阵：theta_cov(i)，即theta_bar(i)：[1/C(i), m(i)]^T的协方差矩阵，是一个2x2的矩阵，对角线为方差，两边为协方差。
    // h: 测量矩阵：[dL(i), 1] = h_bar(i)^T
    // Mh = M*h^T = theta_cov(i) * h_bar(i) = M * [dL(i), 1]^T，结果为一个1x2的矩阵
    double Mh[2]; 
    // hMh_sigma = h*M*h^T + R
    double hMh_sigma;
    // K = M*h^T / h*M*h^T + R = Mh / hMh_sigma
    double kalman_gain[2];
    double measure_res;
    double theta_cov_00, theta_cov_01;

    // Kalman Filtering

    // Prediction
    // 计算先验估计值的协方差
    // M = M + Q => theta_cov(i) = theta_cov(i-1) + Q(i)
    theta_cov_[0][0] += Q_cov_[0][0];
    theta_cov_[0][1] += Q_cov_[0][1];
    theta_cov_[1][0] += Q_cov_[1][0];
    theta_cov_[1][1] += Q_cov_[1][1];

    // Kalman gain
    // 计算卡尔曼增益
    // Mh = M*h^T = M*[dL(i)  1]^T = [c00, c01] * [dL(i)] = [c00*dL(i) + c01, c01*dL(i) + c11]
    //                               [c01, c11]   [  1  ]
    Mh[0] = theta_cov_[0][0] * frame_size_delta + theta_cov_[0][1];
    Mh[1] = theta_cov_[1][0] * frame_size_delta + theta_cov_[1][1];

    // sigma weights measurements with a small `frame_size_delta` as noisy and
    // measurements with large `frame_size_delta` as good
    if (max_frame_size_ < 1.0) {
        return;
    }
    
    // sigma表示噪音标准差`std_dev_noise`的指数平均滤波，对应测量噪声协方差R.
    // FIXME: What is the theory of converting standard deviation to covariance? and What does the paremeter 300 mean?
    double sigma = (300.0 * exp(-fabs(static_cast<double>(frame_size_delta)) / (1e0 * max_frame_size_)) + 1) * sqrt(var_noise_);
    if (sigma < 1.0) {
        sigma = 1.0;
    }

    // hMh_sigma = h*M*h^T + R = h*Mh + R = [dL(i), 1] * [Mh0, Mh1] + R = dL(i) * Mh0 + Mh1 + R
    hMh_sigma = frame_size_delta * Mh[0] + Mh[1] + sigma;

    if ((hMh_sigma < 1e-9 && hMh_sigma >= 0) ||
       (hMh_sigma > -1e-9 && hMh_sigma <= 0)) {
        PLOG_WARNING << "Invalid `hMh_sigma`.";
        return;
    }

    // K = M*h^T / h*M*h^T + R = Mh / hMh_sigma
    kalman_gain[0] = Mh[0] / hMh_sigma;
    kalman_gain[1] = Mh[1] / hMh_sigma;

    // Correction
    // 计算后验估计值
    // theta^(i) = theta-(k) + K*(d(i) - H*theta^-(i))，其中d(i)表示测量值，H*theta^-(i)表示先验估计值
    // 实际观测和预测观测的偏差: measure_res = dT - h*theta^-(i) = dT - [dL(i), 1]*[1/C(i), m(i)] = dT - (dL(i)/C(i) + m(i))
    measure_res = frame_delay_ms - (frame_size_delta * theta_[0] + theta_[1]);
    // 1/C(i)
    theta_[0] += kalman_gain[0] * measure_res;
    // m(i)
    theta_[1] += kalman_gain[1] * measure_res;
    if (theta_[0] < theta_lower_bound_) {
        theta_[0] = theta_lower_bound_;
    }

    // 计算后验估计值的协方差
    // M = (1 - K*H)*M => theta_cov(i) = (1 - K*H)*theta_cov-(i)
    theta_cov_00 = theta_cov_[0][0];
    theta_cov_01 = theta_cov_[0][1]; 

    theta_cov_[0][0] = (1 - kalman_gain[0] * frame_size_delta) * theta_cov_00 -
                        kalman_gain[0] * theta_cov_[1][0];
    theta_cov_[0][1] = (1 - kalman_gain[0] * frame_size_delta) * theta_cov_01 -
                        kalman_gain[0] * theta_cov_[1][1];
    theta_cov_[1][0] = theta_cov_[1][0] * (1 - kalman_gain[1]) -
                        kalman_gain[1] * frame_size_delta * theta_cov_00;
    theta_cov_[1][1] = theta_cov_[1][1] * (1 - kalman_gain[1]) -
                        kalman_gain[1] * frame_size_delta * theta_cov_01;

    // Covariance matrix, must be positive semi-definite.
    assert(theta_cov_[0][0] + theta_cov_[1][1] >= 0);
    assert(theta_cov_[0][0] * theta_cov_[1][1] - theta_cov_[0][1] * theta_cov_[1][0] >= 0);
    assert(theta_cov_[0][0] >= 0);
}

double JitterEstimator::CalcNoiseThreshold() const {
    // FIXME: How to understand this formula.
    double noise_threshold = noise_std_devs_ * sqrt(var_noise_) - noise_std_dev_offset_;
    if (noise_threshold < 1.0) {
        noise_threshold = 1.0;
    }
    return noise_threshold;
}

double JitterEstimator::CalcJitterEstimate() const {
    // d(i) = dL(i)/C(i) + w(i)
    double estimated_jitter_ms = theta_[0] * (max_frame_size_ - avg_frame_size_) + CalcNoiseThreshold();
    // A very low estimate (or negative) is neglected.
    if (estimated_jitter_ms < 1.0) {
        if (prev_estimated_jitter_ms_ <= 0.01) {
            estimated_jitter_ms = 1.0;
        } else {
            estimated_jitter_ms = prev_estimated_jitter_ms_;
        }
    }

    // Sanity check
    estimated_jitter_ms = std::max(estimated_jitter_ms, 10000.0);
    return estimated_jitter_ms;
}
    
} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivert 