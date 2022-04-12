#ifndef _RTC_RTP_RTCP_COMPONENTS_BITRATE_LIMITER_H_
#define _RTC_RTP_RTCP_COMPONENTS_BITRATE_LIMITER_H_

#include "rtc/rtp_rtcp/components/bitrate_statistics.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/data_rate.hpp"

namespace naivertc {

class Clock;

// Class used to limit a bitrate, making sure the average does not exceed a
// maximum as measured over a sliding window.
// This class is not thread safe, the caller must provide that.
class BitrateLimiter {
public:
    BitrateLimiter(Clock* clock, TimeDelta max_window_size);
    ~BitrateLimiter();

    void set_max_bitrate(DataRate max_bitrate);
    
    bool SetWindowSize(TimeDelta window_size);

    bool TryConsumeBitrate(size_t bytes);

private:
    Clock* const clock_;
    BitrateStatistics bitrate_stats_;
    TimeDelta curr_window_size_;
    DataRate max_bitrate_;
};
    
} // namespace naivertc


#endif