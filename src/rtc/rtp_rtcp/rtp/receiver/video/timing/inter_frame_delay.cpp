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
    prev_frame_recv_time_ms_ = 0;
    prev_frame_timestamp_ = 0;
}

std::pair<int64_t, bool> InterFrameDelay::CalculateDelay(uint32_t timestamp, int64_t recv_time_ms) {
    if (prev_frame_recv_time_ms_ == 0) {
        prev_frame_recv_time_ms_ = recv_time_ms;
        prev_frame_timestamp_ = timestamp;
        return {0, true};
    }

    // wrap_around_utils::

    return {0, false};
}

} // namespace video
} // namespace rtp
} // namespace naivert