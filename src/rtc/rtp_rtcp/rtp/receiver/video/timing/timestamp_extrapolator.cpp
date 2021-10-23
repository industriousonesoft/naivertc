#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timestamp_extrapolator.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <limits>

namespace naivertc {
namespace rtc {
namespace video {
namespace {

constexpr double kAlarmThreshold = 60e3;
// in timestamp ticks, i.e. 15 ms
constexpr double kAccDrift = 6600; 
constexpr double kAccMaxError = 7000;
constexpr double kPP11 = 1e10;
constexpr double kLambda = 1;
constexpr uint32_t kStartUpFilterDelayInPackets = 2;
    
} // namespace


TimestampExtrapolator::TimestampExtrapolator(int64_t start_time_ms) 
    : start_time_ms_(0),
      prev_time_ms_(0),
      first_timestamp_(0),
      num_wrap_arounds_(0),
      prev_unrapped_timestamp_(-1),
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
    w_[0] = 90.0;
    w_[1] = 0;
    pP_[0][0] = 1;
    pP_[0][1] = pP_[1][0] = 0;
    pP_[1][1] = kPP11;
    first_after_reset_ = true;
    prev_unrapped_timestamp_ = -1;
    prev_wrap_timestamp_ = std::nullopt;
    num_wrap_arounds_ = 0;
    packet_count_ = 0;
    detector_accumulator_pos_ = 0;
    detector_accumulator_neg_ = 0;
}

void TimestampExtrapolator::Update(uint32_t timestamp, int64_t receive_time_ms) {
    if (receive_time_ms - prev_time_ms_ > 10e3 /* 1s */) {
        // Reset if we has not receive a complete frame after 1s.
        Reset(receive_time_ms);
    } else {
        prev_time_ms_ = receive_time_ms;
    }

    // Remove offset to prevent badly scaled matrices.
    receive_time_ms -= start_time_ms_;

    WrapAroundChecker(timestamp);

    int64_t unwrapped_timestamp = static_cast<int64_t>(timestamp) + num_wrap_arounds_ * std::numeric_limits<uint32_t>::max() /* break point: 2^31 */;

}

void TimestampExtrapolator::WrapAroundChecker(uint32_t timestamp) {
    if (!prev_wrap_timestamp_) {
        prev_wrap_timestamp_ = timestamp;
        return;
    }

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
    prev_wrap_timestamp_ = timestamp;
}
    
} // namespace video
} // namespace rtc
} // namespace naivert 