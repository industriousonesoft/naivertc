#ifndef _RTC_CONGESTION_CONTROL_PACING_TASK_QUEUE_PACED_SENDER_H_
#define _RTC_CONGESTION_CONTROL_PACING_TASK_QUEUE_PACED_SENDER_H_

#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/congestion_control/pacing/pacing_controller.hpp"
#include "rtc/base/task_utils/task_queue.hpp"

namespace naivertc {

class Clock;

class TaskQueuePacedSender : public RtpPacketSender {
public:
    using Configuration = PacingController::Configuration;
public:
    TaskQueuePacedSender(const Configuration& config, 
                         TimeDelta max_hold_back_window = PacingController::kMaxEarlyProbeProcessing,
                         int max_hold_window_in_packets = -1);
    ~TaskQueuePacedSender() override;

    // Implements RtpPacketSender
    void EnqueuePackets(std::vector<RtpPacketToSend> packets) override;

private:
    Clock* const clock_;
    const TimeDelta max_hold_back_window_;
    const int max_hold_window_in_packets_;

    PacingController pacing_controller_;

    TaskQueue task_queue_;

};
    
} // namespace naivertc

#endif