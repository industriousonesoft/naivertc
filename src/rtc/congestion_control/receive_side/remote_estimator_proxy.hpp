#ifndef _RTC_CONGESTION_CONTROL_RECEIVE_SIDE_REMOTE_ESTIMATOR_PROXY_H_
#define _RTC_CONGESTION_CONTROL_RECEIVE_SIDE_REMOTE_ESTIMATOR_PROXY_H_

namespace naivertc {

class Clock;

class RemoteEstimatorProxy {
public:
    RemoteEstimatorProxy(Clock* clock);

private:
    Clock* const clock_;
};
    
} // namespace naivertc


#endif