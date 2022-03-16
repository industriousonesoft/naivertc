#include "rtc/congestion_control/receive_side/remote_estimator_proxy.hpp"

namespace naivertc {

RemoteEstimatorProxy::RemoteEstimatorProxy(const SendFeedbackConfig& send_config,
                                           Clock* clock, 
                                           FeedbackSender* feedback_sender) 
    : send_config_(send_config),
      clock_(clock),
      feedback_sender_(feedback_sender) {
    assert(clock_ != nullptr);
    assert(feedback_sender_ != nullptr);
}
    
} // namespace naivertc
