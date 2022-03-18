#ifndef _RTC_CONGESTION_CONTROL_PACING_TASK_QUEUE_PACED_SENDER_H_
#define _RTC_CONGESTION_CONTROL_PACING_TASK_QUEUE_PACED_SENDER_H_

#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/congestion_control/pacing/pacing_controller.hpp"
#include "rtc/base/numerics/exp_filter.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

#include <mutex>

namespace naivertc {

class Clock;

class TaskQueuePacedSender : public RtpPacketSender {
public:
    using Configuration = PacingController::Configuration;

    // Stats
    struct Stats {
        Timestamp oldest_packet_enqueue_time = Timestamp::MinusInfinity();
        size_t queue_size = 0;
        TimeDelta expected_queue_time = TimeDelta::Zero();
        std::optional<Timestamp> first_sent_packet_time;
    };

public:
    TaskQueuePacedSender(const Configuration& config,
                         TaskQueueImpl* task_queue,
                         TimeDelta max_hold_back_window = PacingController::kMaxEarlyProbeProcessing,
                         int max_hold_window_in_packets = -1);
    ~TaskQueuePacedSender() override;

    void Pause();
    void Resume();

    void EnsureStarted();
    void SetAccountForAudioPackets(bool account_for_audio);
    void SetIncludeOverhead();
    void SetTransportOverhead(size_t overhead_per_packet);
    void SetQueueTimeCap(TimeDelta cap);

    void SetProbingEnabled(bool enabled);
    void SetPacingBitrates(DataRate pacing_bitrate, 
                           DataRate padding_bitrate);
    void SetCongestionWindow(size_t congestion_window_size);
    void OnInflightBytes(size_t inflight_bytes);

    void AddProbeCluster(int cluster_id, DataRate target_bitrate);

    // Implements RtpPacketSender
    void EnqueuePackets(std::vector<RtpPacketToSend> packets) override;

    Stats GetStats();

private:
    void MaybeProcessPackets(Timestamp scheduled_process_time);
    void RescheduleProcess();
    void UpdateStats();

private:
    Clock* const clock_;
    const TimeDelta max_hold_back_window_;
    const int max_hold_window_in_packets_;

    // We want only one (valid) delayed process task in flight at a time.
    // If the value of `next_scheduled_time_` is finite, it is an id for a
    // delayed task that will call MaybeProcessPackets() with that time
    // as parameter.
    // Timestamp::MinusInfinity() indicates no valid pending task.
    Timestamp next_scheduled_process_time_ = Timestamp::MinusInfinity();

    // Indicates if this task queue is started. If not, don't allow
    // posting delayed tasks yet.
    bool is_started_ = false;

    // Indicates if this task queue is shutting down. If so, don't allow
    // posting any more delayed tasks as that can cause the task queue to
    // never drain.
    bool is_shutdown_ = false;

    // Smoothed size of enqueued packtes, in bytes.
    double smoothed_packet_size_ = 0.0;

    Stats current_stats_;
    
    PacingController pacing_controller_;

    TaskQueueImpl* const task_queue_;
};
    
} // namespace naivertc

#endif