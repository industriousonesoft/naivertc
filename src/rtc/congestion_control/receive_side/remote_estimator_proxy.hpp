#ifndef _RTC_CONGESTION_CONTROL_RECEIVE_SIDE_REMOTE_ESTIMATOR_PROXY_H_
#define _RTC_CONGESTION_CONTROL_RECEIVE_SIDE_REMOTE_ESTIMATOR_PROXY_H_

#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_control/receive_side/packet_arrival_time_map.hpp"

#include <vector>
#include <optional>

namespace naivertc {

class Clock;
class RtcpPacket;

class RemoteEstimatorProxy {
public:
    // FeedbackSender
    class FeedbackSender {
    public:
        virtual ~FeedbackSender() = default;
        virtual void SendFeedbacks(std::vector<RtcpPacket> packets) = 0;
    };

    // SendFeedbackConfig
    struct SendFeedbackConfig {
        TimeDelta back_window = TimeDelta::Millis(500);
        TimeDelta min_interval = TimeDelta::Millis(50);
        TimeDelta max_interval = TimeDelta::Millis(250);
        TimeDelta default_interval = TimeDelta::Millis(100);
        double bandwidth_fraction = 0.05;
    };
public:
    RemoteEstimatorProxy(const SendFeedbackConfig& send_config, 
                         Clock* clock, 
                         FeedbackSender* feedback_sender);

private:
    const SendFeedbackConfig send_config_;
    Clock* const clock_;
    FeedbackSender* const feedback_sender_;

    PacketArrivalTimeMap packet_arrival_times_;
};
    
} // namespace naivertc


#endif