#include "rtc/congestion_control/pacing/task_queue_paced_sender.hpp"
#include "rtc/base/time/clock.hpp"

namespace naivertc {
namespace {

constexpr float kDefaultSmoothingCoeff = 0.95;
  
} // namespace

TaskQueuePacedSender::TaskQueuePacedSender(const Configuration& config, 
                                           TimeDelta max_hold_back_window,
                                           int max_hold_window_in_packets) 
    : clock_(config.clock),
      max_hold_back_window_(max_hold_back_window),
      max_hold_window_in_packets_(max_hold_window_in_packets),
      pacing_controller_(config),
      task_queue_("TaskQueuePacedSender.task.queue") {
}
    
TaskQueuePacedSender::~TaskQueuePacedSender() {
    // Post an immediate task to mark the queue as shutting down.
    // The task queue desctructor will wait for pending tasks to
    // complete before continuing.
    task_queue_.Post([this](){
        is_shutdown_ = true;
    });
}

void TaskQueuePacedSender::Pause() {
    task_queue_.Post([this](){
        pacing_controller_.Pause();
    });
}

void TaskQueuePacedSender::Resume() {
    task_queue_.Post([this](){
        pacing_controller_.Resume();
        MaybeProcessPacketsImmediately();
    });
}

void TaskQueuePacedSender::EnsureStarted() {
    task_queue_.Post([this](){
        is_started_ = true;
        MaybeProcessPacketsImmediately();
    });
}

void TaskQueuePacedSender::SetAccountForAudioPackets(bool account_for_audio) {
    task_queue_.Post([this, account_for_audio](){
        pacing_controller_.set_account_for_audio(account_for_audio);
        MaybeProcessPacketsImmediately();
    });
}

void TaskQueuePacedSender::SetIncludeOverhead() {
    task_queue_.Post([this](){
        pacing_controller_.set_include_overhead();
        MaybeProcessPacketsImmediately();
    });
}

void TaskQueuePacedSender::SetTransportOverhead(size_t overhead_per_packet) {
    task_queue_.Post([this, overhead_per_packet](){
        pacing_controller_.set_transport_overhead(overhead_per_packet);
        MaybeProcessPacketsImmediately();
    });
}

void TaskQueuePacedSender::SetQueueTimeCap(TimeDelta cap) {
    task_queue_.Post([this, cap](){
        pacing_controller_.set_queue_time_cap(cap);
        MaybeProcessPacketsImmediately();
    });
}

void TaskQueuePacedSender::AddProbeCluster(int cluster_id, DataRate target_bitrate) {
    task_queue_.Post([this, cluster_id, target_bitrate](){
        pacing_controller_.AddProbeCluster(cluster_id, target_bitrate);
        MaybeProcessPacketsImmediately();
    });
}

void TaskQueuePacedSender::EnqueuePackets(std::vector<RtpPacketToSend> packets) {
    task_queue_.Post([this, packets=std::move(packets)]() mutable {
        for (auto& packet : packets) {
            smoothed_packet_size_ = kDefaultSmoothingCoeff * smoothed_packet_size_ + 
                                    (1 - kDefaultSmoothingCoeff) * packet.size();
            pacing_controller_.EnqueuePacket(std::move(packet));
        }
        MaybeProcessPacketsImmediately();
    });
}

// Private methods
void TaskQueuePacedSender::MaybeProcessPackets(Timestamp scheduled_process_time) {
    RTC_RUN_ON(&task_queue_);

    if (is_shutdown_ || !is_started_) {
        return;
    }

    // Normally, call ProcessPackets() only if this is the scheduled task.
    // If it's not but it's already time to process and there either is 
    // no scheduled task or the schedule has shifted forward in time, run
    // anyway and clear any schedule.
    Timestamp next_process_time = pacing_controller_.NextSendTime();
    const auto now = clock_->CurrentTime();
    const bool is_scheduled_call = next_scheduled_process_time_ == scheduled_process_time;
    if (is_scheduled_call) {
        // Indicates no pending scheduled call.
        next_scheduled_process_time_ = Timestamp::MinusInfinity();
    }
    // Check if it's time to process paced packets.
    if (is_scheduled_call ||
        (now >= next_process_time && (next_scheduled_process_time_.IsInfinite() ||
                                      next_process_time < next_scheduled_process_time_))) {
        pacing_controller_.ProcessPackets();
        next_process_time = pacing_controller_.NextSendTime();
    }

    // Update holdback window.
    TimeDelta hold_back_window = max_hold_back_window_;
    DataRate pacing_bitrate = pacing_controller_.pacing_bitrate();
    if (max_hold_window_in_packets_ > 0 &&
        !pacing_bitrate.IsZero() &&
        smoothed_packet_size_ > 0) {
        // Using double for high precision. 
        TimeDelta avg_packet_send_time = TimeDelta::Millis(smoothed_packet_size_ * 8000 / pacing_bitrate.bps<double>());
        hold_back_window = std::min(hold_back_window, avg_packet_send_time * max_hold_window_in_packets_);
    }

    std::optional<TimeDelta> time_to_next_process;
    if (pacing_controller_.IsProbing() && 
        next_process_time != next_scheduled_process_time_) {
        // If we're probing and there isn't already a wakeup scheduled for the 
        // next process time, always post a task and just round sleep time down
        // to the nearest millisecond.
        if (next_process_time.IsMinusInfinity()) {
            time_to_next_process = TimeDelta::Zero();
        } else {
            time_to_next_process = std::max(TimeDelta::Zero(), (next_process_time - now).RoundDownTo(TimeDelta::Millis(1)));
        }
    } else if (next_scheduled_process_time_.IsMinusInfinity() ||
               next_process_time <= next_scheduled_process_time_ - hold_back_window) {
        // Schdule a new task since there is none currently scheduled (|next_scheduled_process_time_| is infinite),
        // or the new process is at least one holdback window earlier than whatever is currently scheduled.
        time_to_next_process = std::max(next_process_time - now, hold_back_window);
    }

    if (time_to_next_process) {
        // Set a new scheduled process time and post a delayed task.
        next_scheduled_process_time_ = next_process_time;

        // Schedule the next process.
        task_queue_.PostDelayed(*time_to_next_process, [this, next_process_time](){
            MaybeProcessPackets(next_process_time);
        });
    }

    UpdateStats();
}

void TaskQueuePacedSender::MaybeProcessPacketsImmediately() {
     MaybeProcessPackets(Timestamp::MinusInfinity());
}

void TaskQueuePacedSender::UpdateStats() {
    RTC_RUN_ON(&task_queue_);
    Stats new_stats;
    new_stats.expected_queue_time = pacing_controller_.ExpectedQueueTime();
    new_stats.first_sent_packet_time = pacing_controller_.first_sent_packet_time();
    new_stats.oldest_packet_enqueue_time = pacing_controller_.OldestPacketEnqueueTime();
    new_stats.queue_size = pacing_controller_.QueuedPacketSize();
    current_stats_ = new_stats;
}

TaskQueuePacedSender::Stats TaskQueuePacedSender::GetStats() const {
    RTC_RUN_ON(&task_queue_);
    return current_stats_;
}
    
} // namespace naivertc
