#ifndef _RTC_RTP_RTCP_COMPONENTS_STREAM_STATISTICIAN_H_
#define _RTC_RTP_RTCP_COMPONENTS_STREAM_STATISTICIAN_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpStreamStatistician {
public:
    RtpStreamStatistician(uint32_t ssrc, Clock* clock, int max_reordering_threshold);
    ~RtpStreamStatistician();

private:
    const uint32_t ssrc_;
    Clock* const clock_;
    // Delta used to map internal timestamps to Unix epoch ones.
    const int64_t delta_internal_unix_epoch_ms_;
};
    
} // namespace naivertc


#endif