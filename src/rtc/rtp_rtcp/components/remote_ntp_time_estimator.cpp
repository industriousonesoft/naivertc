#include "rtc/rtp_rtcp/components/remote_ntp_time_estimator.hpp"

namespace naivertc {

RemoteNtpTimeEstimator::RemoteNtpTimeEstimator(std::shared_ptr<Clock> clock) 
    : clock_(std::move(clock)) {}

RemoteNtpTimeEstimator::~RemoteNtpTimeEstimator() {}
    
} // namespace naivertc
