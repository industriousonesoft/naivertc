#ifndef _RTC_RTP_RTCP_COMPONENTS_REMOTE_NTP_TIME_ESTIMATOR_H_
#define _RTC_RTP_RTCP_COMPONENTS_REMOTE_NTP_TIME_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock_real_time.hpp"

#include <memory>

namespace naivertc {

// RemoteNtpTimeEstimator can be used to estimate 
// a given RTP timestamp's NTP time in local timebase.
class RTC_CPP_EXPORT RemoteNtpTimeEstimator {
public:
    explicit RemoteNtpTimeEstimator(std::shared_ptr<Clock> clock);
    ~RemoteNtpTimeEstimator();

private:
    std::shared_ptr<Clock> clock_;
};
    
} // namespace naivertc


#endif