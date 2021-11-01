#ifndef _RTC_RTP_RTCP_COMPONENTS_REMOTE_NTP_TIME_ESTIMATOR_H_
#define _RTC_RTP_RTCP_COMPONENTS_REMOTE_NTP_TIME_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock_real_time.hpp"
#include "rtc/base/numerics/moving_median_filter.hpp"
#include "rtc/rtp_rtcp/components/rtp_to_ntp_estimator.hpp"

#include <memory>

namespace naivertc {

// RemoteNtpTimeEstimator can be used to estimate 
// a given RTP timestamp's NTP time in local timebase.
// It will create a linear regression between timestamp and NTP time
// based on SR pakcet, then the linear regression is used to estimate 
// the RTP packet received NTP time in local timebase.
class RTC_CPP_EXPORT RemoteNtpTimeEstimator {
public:
    explicit RemoteNtpTimeEstimator(std::shared_ptr<Clock> clock);
    ~RemoteNtpTimeEstimator();

    bool UpdateRtcpTimestamp(int64_t rtt, 
                             uint32_t ntp_secs, 
                             uint32_t ntp_frac, 
                             uint32_t rtp_timestamp);

    // Estimates the NTP timestamp in local timebase from |rtp_timestamp|.
    // Returns the NTP timestamp in ms when success. -1 if failed.
    int64_t Estimate(uint32_t rtp_timestamp);

    // Estimates the offset, in milliseconds, between the remote clock and the
    // local one. This is equal to local NTP clock - remote NTP clock.
    std::optional<int64_t> EstimateRemoteToLocalClockOffsetMs();

private:
    std::shared_ptr<Clock> clock_;
    MovingMedianFilter<int64_t> ntp_clocks_offset_estimator_;
    RtpToNtpEstimator rtp_to_ntp_estimator_;
    int64_t last_timing_log_ms_;
};
    
} // namespace naivertc


#endif