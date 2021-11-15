#include "rtc/rtp_rtcp/rtp/receiver/video/timing/inter_frame_delay.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

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
    // Detects if the timestamp clock has overflowd since the last timestamp
    // and keep track of the number of wrap arounds since last.
    num_wrap_around_ += wrap_around_utils::DetectWrapAround<uint32_t>(prev_timestamp_, timestamp);

    // This will be -1 for backward wrap arounds and +1 for forward wrap arounds.
    int32_t wrap_arounds_since_prev = num_wrap_around_ - prev_num_wrap_around;

    // Check if backward or backward wrap around happened.
    if ((wrap_arounds_since_prev == 0 && timestamp < prev_timestamp_) || wrap_arounds_since_prev < 0) {
        // Account for reordering in jitter variance estimate in the future?
        // Note that this also captures incomplete frames which are grabbed for
        // decoding after a later frame has been complete, i.e. real packet losses.
        return {0, false};
    }

    // The count from 0 to the max of type T.
    constexpr int64_t kRoundTimestamp = static_cast<int64_t>(1) << 32; // int64_t{std::numeric_limits<uint32_t>::max()} + 1;
    // Compute the compensated timestamp difference and covert it to ms and round
    // it to the closest integer.
    diff_timestamp_ = static_cast<int64_t>((timestamp + wrap_arounds_since_prev * kRoundTimestamp - prev_timestamp_) / 90.0 + 0.5);

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

} // namespace video
} // namespace rtp
} // namespace naivert