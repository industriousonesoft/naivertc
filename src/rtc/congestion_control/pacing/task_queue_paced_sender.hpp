#ifndef _RTC_CONGESTION_CONTROL_PACING_TASK_QUEUE_PACED_SENDER_H_
#define _RTC_CONGESTION_CONTROL_PACING_TASK_QUEUE_PACED_SENDER_H_

#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/congestion_control/pacing/pacing_controller.hpp"
#include "rtc/base/numerics/exp_filter.hpp"
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
    struct Stats {
        Timestamp oldest_packet_enqueue_time = Timestamp::MinusInfinity();
        size_t queue_size = 0;
        TimeDelta expected_queue_time = TimeDelta::Zero();
        std::optional<Timestamp> first_sent_packet_time;
    };

private:
    Clock* const clock_;
    const TimeDelta max_hold_back_window_;
    const int max_hold_window_in_packets_;

    // We want only one (valid) delayed process task in flight at a time.
    // If the value of `next_process_time_` is finite, it is an id for a
    // delayed task that will call MaybeProcessPackets() with that time
    // as parameter.
    // Timestamp::MinusInfinity() indicates no valid pending task.
    Timestamp next_process_time_ = Timestamp::MinusInfinity();

    // Indicates if this task queue is started. If not, don't allow
    // posting delayed tasks yet.
    bool is_started_ = false;

    // Indicates if this task queue is shutting down. If so, don't allow
    // posting any more delayed tasks as that can cause the task queue to
    // never drain.
    bool is_shutdown_ = false;
    
    PacingController pacing_controller_;

    // Filtered size of enqueued packtes, in bytes.
    ExpFilter packet_size_;

    TaskQueue task_queue_;

};
    
} // namespace naivertc

#endif