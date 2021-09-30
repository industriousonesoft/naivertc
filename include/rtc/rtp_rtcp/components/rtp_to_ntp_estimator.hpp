#ifndef _RTC_RTP_RTCP_COMPONENTS_TIME_RTP_TO_NTP_ESTIMATOR_H_
#define _RTC_RTP_RTCP_COMPONENTS_TIME_RTP_TO_NTP_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/time/ntp_time.hpp"

namespace naivertc {

// Class for converting an RTP timestamp to the NTP domain in milliseconds.
class RTC_CPP_EXPORT RtpToNtpEstimator {
public:
    struct Measurement {
        NtpTime ntp_time;
        int64_t unwrapped_rtp_timestamp;
    };
public:
    RtpToNtpEstimator();
    ~RtpToNtpEstimator();

};

} // namespace naivertc

#endif