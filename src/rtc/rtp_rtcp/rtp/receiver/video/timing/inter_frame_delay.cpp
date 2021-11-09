#include "rtc/rtp_rtcp/rtp/receiver/video/timing/inter_frame_delay.hpp"

namespace naivertc {
namespace rtp {
namespace video {

InterFrameDelay::InterFrameDelay() {
    Reset();
}

InterFrameDelay::~InterFrameDelay() {}

void InterFrameDelay::Reset() {
    prev_recv_time_ms_ = 0;
    prev_timestamp_ = 0;
    num_wrap_around_ = 0;
}

std::pair<int64_t, bool> InterFrameDelay::CalculateDelay(uint32_t timestamp, int64_t recv_time_ms) {
    if (prev_recv_time_ms_ == 0) {
        prev_recv_time_ms_ = recv_time_ms;
        prev_timestamp_ = timestamp;
        return {0, true};
    }

    int32_t prev_num_wrap_around = num_wrap_around_;
    CheckForWrapArounds(timestamp);

    // This will be -1 for backward wrap arounds and +1 for forward wrap arounds.
    int32_t wrap_arounds_since_prev = num_wrap_around_ - prev_num_wrap_around;

    // Check if backward or backward wrap around happened.
    if ((wrap_arounds_since_prev == 0 && timestamp < prev_timestamp_) || wrap_arounds_since_prev < 0) {
        // Account for reordering in jitter variance estimate in the future?
        // Note that this also captures incomplete frames which are grabbed for
        // decoding after a later frame has been complete, i.e. real packet losses.
        return {0, false};
    }

    constexpr int64_t kModuloValue = int64_t{std::numeric_limits<uint32_t>::max() + 1};
    int64_t unwrapped_timestamp = timestamp + wrap_arounds_since_prev * kModuloValue;
    // Compute the compensated timestamp difference and covert it to ms and round
    // it to the closest integer.
    diff_timestamp_ = static_cast<int64_t>((unwrapped_timestamp - prev_timestamp_) / 90.0 + 0.5);

    // Frame delay is the difference of dT and dTS.
    // T1
    //     ------                     
    //           -------    t1
    // T2
    //     ------
    //           -------    t2 
    // Delay = dT - dTS = (t2-t1) - (T2-T1)
    int64_t delay = static_cast<int64_t>(recv_time_ms - prev_recv_time_ms_ - diff_timestamp_);

    prev_timestamp_ = timestamp;
    prev_recv_time_ms_ = recv_time_ms;

    return {delay, true};
}

// Private methods
void InterFrameDelay::CheckForWrapArounds(uint32_t timestamp) {
    if (timestamp < prev_timestamp_) {
        // This difference will probably be less than -2^31 if we have had a forward wrap
        // around (e.g. timestamp = 1, _prevTimestamp = 2^32 - 1). 
        // Since it is cast to a int32_t, it should be positive.
        // A - B = A + (B的补码) = A +（~B+1) = A + (2^n-1-B+1) = A + (2^n-B) = A - B + 2^n
        // ForwardDiff: prev_timestamp_ -> timestamp
        if (static_cast<int32_t>(timestamp - prev_timestamp_) > 0) {
            // Forward wrap around
            ++num_wrap_around_;
        }
    } else {
        // This difference will probably be less than -2^31 if we have had a
        // backward wrap around. 
        // Since it is cast to a int32_t, it should be positive.
        // ReverseDiff: timestamp <- prev_timestamp_
        if (static_cast<int32_t>(prev_timestamp_ - timestamp) > 0) {
            // Backward wrap around
            --num_wrap_around_;
        }
    }
}

} // namespace video
} // namespace rtp
} // namespace naivert