#include "rtc/congestion_control/receive_side/remote_estimator_proxy.hpp"

namespace naivertc {

RemoteEstimatorProxy::RemoteEstimatorProxy(Clock* clock) 
    : clock_(clock) {}
    
} // namespace naivertc
