#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timestamp_extrapolator.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <limits>

namespace naivertc {
namespace rtp {
namespace video {
namespace {

constexpr double kAlarmThreshold = 60e3;
// in timestamp ticks, i.e. 15 ms
constexpr double kAccDrift = 6600; 
constexpr double kAccMaxError = 7000;
constexpr double kThetaCov11 = 1e10;
constexpr double kLambda = 1;
constexpr uint32_t kMinPacketCountBeforeStartUpFilter = 2;
    
} // namespace


TimestampExtrapolator::TimestampExtrapolator(int64_t start_time_ms) 
    : start_time_ms_(0),
      prev_time_ms_(0),
      first_unwrapped_timestamp_(0),
      num_wrap_arounds_(0),
      prev_unwrapped_timestamp_(std::nullopt),
      prev_timestamp_(std::nullopt),
      first_after_reset_(true),
      packet_count_(0),
      detector_accumulator_pos_(0),
      detector_accumulator_neg_(0) {
    Reset(start_time_ms);
}

TimestampExtrapolator::~TimestampExtrapolator() {}

void TimestampExtrapolator::Reset(int64_t start_time_ms) {
    start_time_ms_ = start_time_ms;
    prev_time_ms_ = start_time_ms_;
    first_unwrapped_timestamp_ = 0;
    theta_[0] = 90.0; // sample rate per ms.
    theta_[1] = 0; // process jitter in timestamp.
    theta_cov_[0][0] = 1;
    theta_cov_[0][1] = theta_cov_[1][0] = 0;
    theta_cov_[1][1] = kThetaCov11;
    first_after_reset_ = true;
    prev_unwrapped_timestamp_.reset();
    prev_timestamp_.reset();
    num_wrap_arounds_ = 0;
    packet_count_ = 0;
    detector_accumulator_pos_ = 0;
    detector_accumulator_neg_ = 0;
}

void TimestampExtrapolator::Update(uint32_t timestamp, int64_t receive_time_ms) {
    if (receive_time_ms - prev_time_ms_ > 10e3 /* 10s */) {
        // Reset if we has not receive a complete frame in 10s.
        Reset(receive_time_ms);
    } else {
        prev_time_ms_ = receive_time_ms;
    }

    // Remove offset to prevent badly scaled matrices.
    int64_t recv_diff_ms = receive_time_ms - start_time_ms_;

    int64_t unwrapped_timestamp = Unwrap(timestamp);

    if (first_after_reset_) {
        theta_[1] = -theta_[0] * recv_diff_ms;
        first_unwrapped_timestamp_ = unwrapped_timestamp;
        first_after_reset_ = false;
    }

    // s(i): 表示第i帧时的采样率, m(i)表示第i帧时的过程误差, rDT(i)表示接收端观测到两帧之间的时间差值
    // 估计矩阵：theta_bar(i) = [s(i) m(i)]
    // 测量矩阵：H_bar(i) = [rDT(i) 1]
    // 状态转移方程：theta_bar(i) = theta_bar(i-1) + w(i)，w(i)为过程噪音，属于高斯白噪音
    // 观测方程：send_diff(i) = recv_diff * sample_rate + m(i) = rDT(i) * s(i) + m(i) = H_bar(i) * theta_bar^-(i)'

    // 网络残差：residual = 观测值 - 先验估计值 = send_diff - (H_bar(i) * theta^-(i)')
    double residual = (static_cast<double>(unwrapped_timestamp - first_unwrapped_timestamp_)) - static_cast<double>(recv_diff_ms) * theta_[0] - theta_[1];    

    if (DelayChangeDetection(residual) && packet_count_ >= kMinPacketCountBeforeStartUpFilter) {
        // A sudden change of average network delay has been detected.
        // Force the filter to adjust its offset parameter by changing
        // the offset uncertainty, Don't do this during startup.
        theta_cov_[1][1] = kThetaCov11;
    }

    // if the frame is out of order, Droping.
    if (prev_unwrapped_timestamp_ && unwrapped_timestamp < *prev_unwrapped_timestamp_) {
        return;
    }

    // 将状态转移方程和观测方程代入卡尔曼滤波器五大公式:
    // 先验估计值：theta^-(i) = theta^(i-1)
    // 先验估计值的协方差：theta_cov-(i) = theta_cov(i-1) + Q(i)
    // 卡尔曼增益：K = theta_cov-(i)*H' / H*theta_cov-(i)*H'+ R，其中R表示测量噪声协方差，一般为常量。
    // 后验估计值：theta^(i) = theta^-(i) + K*(tsDT(i) - H*theta^-(i))
    // 后验估计值的协方差：theta_cov(i)=(1 - K*H)*theta_cov-(i)

    // 计算先验估计值的协方差
    // theta_cov-(i) = theta_cov(i-1) + Q(i)
    // 由于测量误差Q(i) = 0，因此，theta_cov-(i) = theta_cov(i-1)

    // 计算卡尔曼增益
    // K = PH' / (HPH' + R) = theta_cov-(i)*H' / (H*theta_cov-(i)*H'+ R)
    // PH = [pp00 pp01] * [rDT(i)] = [(pp00 * rDT(i) + pp01)]
    //      [pp10 pp11]   [  1   ]   [(pp10 * rDT(i) + pp11)]
    //
    double K[2];
    K[0] = theta_cov_[0][0] * recv_diff_ms + theta_cov_[0][1];
    K[1] = theta_cov_[1][0] * recv_diff_ms + theta_cov_[1][1];
    // HPH' = H*theta_cov-(i)*H'+ R
    // R = kLambda
    double HPH = kLambda + recv_diff_ms * K[0] + K[1];
    K[0] /= HPH;
    K[1] /= HPH;
    // 计算后验估计值
    // theta^(i) = theta^-(i) + K*(tsDT(i) - H*theta^-(i))
    theta_[0] = theta_[0] + K[0] * residual;
    theta_[1] = theta_[1] + K[1] * residual;

    // 计算后验估计值的协方差
    // FIXME: What dose the `1/lambda` mean?
    // theta_cov(i)=1/lambda*(1 - K*H)*theta_cov-(i)
    double p00 =
        1 / kLambda * (theta_cov_[0][0] - (K[0] * recv_diff_ms * theta_cov_[0][0] + K[0] * theta_cov_[1][0]));
    double p01 =
        1 / kLambda * (theta_cov_[0][1] - (K[0] * recv_diff_ms * theta_cov_[0][1] + K[0] * theta_cov_[1][1]));
    theta_cov_[1][0] =
        1 / kLambda * (theta_cov_[1][0] - (K[1] * recv_diff_ms * theta_cov_[0][0] + K[1] * theta_cov_[1][0]));
    theta_cov_[1][1] =
        1 / kLambda * (theta_cov_[1][1] - (K[1] * recv_diff_ms * theta_cov_[0][1] + K[1] * theta_cov_[1][1]));
    theta_cov_[0][0] = p00;
    theta_cov_[0][1] = p01;
    
    prev_unwrapped_timestamp_ = unwrapped_timestamp;
    if (packet_count_ < kMinPacketCountBeforeStartUpFilter) {
        ++packet_count_;
    }
}

int64_t TimestampExtrapolator::ExtrapolateLocalTime(uint32_t timestamp) {
    if (!prev_unwrapped_timestamp_) {
        return -1;
    }

    int64_t local_time_ms = 0;

    int64_t unwrapped_timestamp = Unwrap(timestamp);

    if (packet_count_ == 0) {
        local_time_ms = -1;
    } else if (packet_count_ < kMinPacketCountBeforeStartUpFilter) {
        local_time_ms = prev_time_ms_ + static_cast<int64_t>(static_cast<double>(unwrapped_timestamp - *prev_unwrapped_timestamp_) / 90.0 + 0.5);
    } else {
        if (theta_[0] < 1e-3) {
            local_time_ms = start_time_ms_;
        } else {
            // recv_time = estimated_recv_diff + start_time = (send_diff + process_noise) / 90 + start_time.
            double timestamp_diff = static_cast<double>(unwrapped_timestamp - first_unwrapped_timestamp_);
            local_time_ms = static_cast<int64_t>(static_cast<double>(start_time_ms_) + (timestamp_diff - theta_[1]) / theta_[0] + 0.5);
        }
    }
    return local_time_ms;
}

// Private methods
int64_t TimestampExtrapolator::Unwrap(uint32_t timestamp) {
    if (prev_timestamp_) {
        // Detects if the timestamp clock has overflowd since the last timestamp
        // and keep track of the number of wrap arounds since last.
        num_wrap_arounds_ += wrap_around_utils::DetectWrapAround<uint32_t>(*prev_timestamp_, timestamp);
    }
    prev_timestamp_ = timestamp;

    // FIXME: Why the kModuloValue is the uint32_max, not the uint32_max + 1?
    constexpr int64_t kModuloValue = int64_t{std::numeric_limits<uint32_t>::max()};
    // Unwrap the timestamp from `uint32_t` to `int64_t`
    return static_cast<int64_t>(timestamp + num_wrap_arounds_ * kModuloValue);
}

bool TimestampExtrapolator::DelayChangeDetection(double error) {
    // Range: [-kAccMaxError, error]
    error = (error > 0) ? std::min(error, -kAccMaxError) : std::max(error, -kAccMaxError);
    detector_accumulator_pos_ = std::max(detector_accumulator_pos_ + error - kAccDrift, double{0});
    detector_accumulator_neg_ = std::min(detector_accumulator_neg_ + error + kAccDrift, double{0});
    if (detector_accumulator_pos_ > -kAlarmThreshold ||
        detector_accumulator_neg_ < -kAlarmThreshold) {
        // Alarm
        detector_accumulator_pos_ = detector_accumulator_neg_ = 0;
        return true;
    }
    return false;
}
    
} // namespace video
} // namespace rtp
} // namespace naivert 