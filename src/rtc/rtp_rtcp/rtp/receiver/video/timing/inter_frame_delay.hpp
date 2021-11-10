#ifndef _RTC_RTP_RTCP_RECEIVER_VIDEO_TIMING_INTER_FRAME_DELAY_H_
#define _RTC_RTP_RTCP_RECEIVER_VIDEO_TIMING_INTER_FRAME_DELAY_H_

#include "base/defines.hpp"

namespace naivertc {
namespace rtp {
namespace video {

// This class used to calculate the delay of a complete frame, which is the interval time
// between the timestamp and the last received packet.
class RTC_CPP_EXPORT InterFrameDelay {
public:
    InterFrameDelay();
    ~InterFrameDelay();

    void Reset();

    std::pair<int64_t, bool> CalculateDelay(uint32_t timestamp, int64_t recv_time_ms);

private:
    int64_t prev_recv_time_ms_;
    uint32_t prev_timestamp_;
    int32_t num_wrap_around_;
    int64_t diff_timestamp_;
};
    
} // namespace video
} // namespace rtp
} // namespace naivert 


#endif