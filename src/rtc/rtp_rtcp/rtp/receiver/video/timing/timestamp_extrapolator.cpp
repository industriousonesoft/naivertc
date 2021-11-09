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
constexpr double kPP11 = 1e10;
constexpr double kLambda = 1;
constexpr uint32_t kMinPacketCountBeforeStartUpFilter = 2;
    
} // namespace


TimestampExtrapolator::TimestampExtrapolator(int64_t start_time_ms) 
    : start_time_ms_(0),
      prev_time_ms_(0),
      first_timestamp_(0),
      num_wrap_arounds_(0),
      prev_unwrapped_timestamp_(std::nullopt),
      prev_wrap_timestamp_(std::nullopt),
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
    first_timestamp_ = 0;
    w_[0] = 90.0; // sample rate per ms.
    w_[1] = 0; // jitter in timestamp
    pP_[0][0] = 1;
    pP_[0][1] = pP_[1][0] = 0;
    pP_[1][1] = kPP11;
    first_after_reset_ = true;
    prev_unwrapped_timestamp_.reset();
    prev_wrap_timestamp_.reset();
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
    int64_t receive_time_ms_diff = receive_time_ms - start_time_ms_;

    int64_t unwrapped_timestamp = Unwrap(timestamp);

    if (first_after_reset_) {
        w_[1] = -w_[0] * receive_time_ms_diff;
        first_timestamp_ = unwrapped_timestamp;
        first_after_reset_ = false;
    }

    double residual = (static_cast<double>(unwrapped_timestamp) - first_timestamp_) - static_cast<double>(receive_time_ms_diff) * w_[0] - w_[1];    

    if (DelayChangeDetection(residual) && packet_count_ >= kMinPacketCountBeforeStartUpFilter) {
        // A sudden change of average network delay has been detected.
        // Force the filter to adjust its offset parameter by changing
        // the offset uncertainty, Don't do this during startup.
        pP_[1][1] = kPP11;
    }

    // if the frame is out of order, Droping.
    if (prev_unwrapped_timestamp_ && unwrapped_timestamp < *prev_unwrapped_timestamp_) {
        return;
    }

    // T = [t(k) 1]';
    // that = T'*w;
    // K = P*T/(lambda + T'*P*T);
    double K[2];
    K[0] = pP_[0][0] * receive_time_ms_diff + pP_[0][1];
    K[1] = pP_[1][0] * receive_time_ms_diff + pP_[1][1];
    double TPT = kLambda + receive_time_ms_diff * K[0] + K[1];
    K[0] /= TPT;
    K[1] /= TPT;
    // w = w + K*(ts(k) - that);
    w_[0] = w_[0] + K[0] * residual;
    w_[1] = w_[1] + K[1] * residual;
    // P = 1/lambda*(P - K*T'*P);
    double p00 =
        1 / kLambda * (pP_[0][0] - (K[0] * receive_time_ms_diff * pP_[0][0] + K[0] * pP_[1][0]));
    double p01 =
        1 / kLambda * (pP_[0][1] - (K[0] * receive_time_ms_diff * pP_[0][1] + K[0] * pP_[1][1]));
    pP_[1][0] =
        1 / kLambda * (pP_[1][0] - (K[1] * receive_time_ms_diff * pP_[0][0] + K[1] * pP_[1][0]));
    pP_[1][1] =
        1 / kLambda * (pP_[1][1] - (K[1] * receive_time_ms_diff * pP_[0][1] + K[1] * pP_[1][1]));
    pP_[0][0] = p00;
    pP_[0][1] = p01;
    
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

    double unwrapped_timestamp = Unwrap(timestamp);

    if (packet_count_ == 0) {
        local_time_ms = -1;
    } else if (packet_count_ < kMinPacketCountBeforeStartUpFilter) {
        local_time_ms = prev_time_ms_ + static_cast<int64_t>(static_cast<double>(unwrapped_timestamp - *prev_unwrapped_timestamp_) / 90.0 + 0.5);
    } else {
        if (w_[0] < 1e-3) {
            local_time_ms = start_time_ms_;
        } else {
            double timestamp_diff = unwrapped_timestamp - static_cast<double>(first_timestamp_);
            local_time_ms = static_cast<int64_t>(static_cast<double>(start_time_ms_) + (timestamp_diff - w_[1]) / w_[0] + 0.5);
        }
    }
    return local_time_ms;
}

// Private methods
int64_t TimestampExtrapolator::Unwrap(uint32_t timestamp) {
    if (prev_wrap_timestamp_) {
        if (timestamp < *prev_wrap_timestamp_) {
            // This difference of the smaller one subtract the bigger one will probably 
            // be less than -2^31 if we have had a wrap around (e.g. timestamp = 1, _previousTimestamp = 2^32 - 1).
            // Since it is casted to a Word32, then the highest bit used as sign bit, it should be positive.
            if (static_cast<int32_t>(timestamp - *prev_wrap_timestamp_) > 0) {
                // Forward wrap around
                num_wrap_arounds_++;
            }
        } else {
            // This difference of the smaller one subtract the bigger will probably 
            // be less than -2^31 if we have had a backward wrap around.
            // Since it is casted to a Word32, it should be positive.
            if (static_cast<int32_t>(*prev_wrap_timestamp_ - timestamp) > 0) {
                // Backward wrap around
                num_wrap_arounds_--;
            }
        }
    }
    prev_wrap_timestamp_ = timestamp;

    constexpr int64_t kModuloValue = int64_t{std::numeric_limits<uint32_t>::max() + 1};
    // Unwrap the timestamp from `uint32_t` to `int64_t`
    return static_cast<int64_t>(timestamp) + num_wrap_arounds_ * kModuloValue;
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