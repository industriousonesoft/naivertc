#include "rtc/base/task_utils/repeating_task.hpp"

namespace naivertc {

std::unique_ptr<RepeatingTask> RepeatingTask::DelayedStart(Clock* clock,
                                                           TaskQueueImpl* task_queue,
                                                           TimeDelta delay, 
                                                           TaskClouser task_clouser) {
    auto task = std::unique_ptr<RepeatingTask>(new RepeatingTask(clock, task_queue, std::move(task_clouser)));
    task->Start(delay);
    return task;
}

RepeatingTask::RepeatingTask(Clock* clock,
                             TaskQueueImpl* task_queue,
                             TaskClouser task_clouser) 
    : clock_(clock),
      task_queue_(task_queue),
      task_clouser_(std::move(task_clouser)),
      is_stoped_(true) {
    assert(task_queue != nullptr);
}

RepeatingTask::~RepeatingTask() = default;

void RepeatingTask::Start(TimeDelta delay) {
    task_queue_->Post([this, delay](){
        this->is_stoped_ = false;
        if (delay.ms() <= 0) {
            this->ExecuteTask();
        } else {
            this->ScheduleTaskAfter(delay);
        }
    });
}

bool RepeatingTask::Running() const {
    return task_queue_->Invoke<bool>([this](){
        return !is_stoped_;
    });
}

void RepeatingTask::Stop() {
    task_queue_->Invoke<void>([this](){
        is_stoped_ = true;
    });
}

// Private methods
void RepeatingTask::ScheduleTaskAfter(TimeDelta delay) {
    RTC_RUN_ON(task_queue_);
    Timestamp execution_time = clock_->CurrentTime() + delay;
    task_queue_->PostDelayed(delay, [this, execution_time](){
        this->MaybeExecuteTask(execution_time);
    });
}

void RepeatingTask::MaybeExecuteTask(Timestamp execution_time) {
    RTC_RUN_ON(task_queue_);
    if (this->is_stoped_) {
        return;
    }
    Timestamp now = clock_->CurrentTime();
    if (now >= execution_time) {
        ExecuteTask();
        return;
    }

    PLOG_WARNING << "RepeatingTask: scheduled delayed called too early.";
    TimeDelta delay = execution_time - now;
    task_queue_->PostDelayed(delay, [this, execution_time](){
        this->MaybeExecuteTask(execution_time);
    });
}

void RepeatingTask::ExecuteTask() {
    RTC_RUN_ON(task_queue_);
    if (this->task_clouser_) {
        TimeDelta interval = this->task_clouser_();
        if (interval.ms() > 0) {
            ScheduleTaskAfter(interval);
        } else {
            // Stop internally if the `interval` is not a positive number.
            this->is_stoped_ = true;
        }
    }
}
    
} // namespace naivertc
