#include "rtc/base/task_utils/repeating_task.hpp"

namespace naivertc {

std::unique_ptr<RepeatingTask> RepeatingTask::DelayedStart(std::shared_ptr<Clock> clock, 
                                                           std::shared_ptr<TaskQueue> task_queue, 
                                                           TimeDelta delay, 
                                                           TaskHandler handler) {
    auto task = std::unique_ptr<RepeatingTask>(new RepeatingTask(clock, task_queue, std::move(handler)));
    task->Start(delay);
    return task;
}

RepeatingTask::RepeatingTask(std::shared_ptr<Clock> clock, 
                             std::shared_ptr<TaskQueue> task_queue,
                             TaskHandler handler) 
    : clock_(clock),
      task_queue_(task_queue != nullptr ? std::move(task_queue) 
                                        : std::make_shared<TaskQueue>("RepeatingTask.default.task.queue")),
      handler_(std::move(handler)) {}

RepeatingTask::~RepeatingTask() {
    Stop();
}

void RepeatingTask::Start(TimeDelta delay) {
    task_queue_->Async([this, delay=std::move(delay)](){
        this->is_stoped = false;
        if (delay.ms() <= 0) {
            this->ExecuteTask();
        } else {
            this->ScheduleTaskAfter(delay);
        }
    });
}

bool RepeatingTask::Running() const {
    return task_queue_->Sync<bool>([this](){
        return !this->is_stoped;
    });
}

void RepeatingTask::Stop() {
    if (task_queue_->IsCurrent()) {
        is_stoped = true;
    } else {
        task_queue_->Async([this](){
            this->is_stoped = true;
        });
    }
}

// Private methods
void RepeatingTask::ScheduleTaskAfter(TimeDelta delay) {
    Timestamp execution_time = clock_->CurrentTime() + delay;
    task_queue_->AsyncAfter(delay, [this, execution_time](){
        this->MaybeExecuteTask(execution_time);
    });
}

void RepeatingTask::MaybeExecuteTask(Timestamp execution_time) {
    if (this->is_stoped) {
        return;
    }
    Timestamp now = clock_->CurrentTime();
    if (now >= execution_time) {
        ExecuteTask();
        return;
    }

    PLOG_WARNING << "RepeatingTask: scheduled delayed called too early.";
    TimeDelta delay = execution_time - now;
    task_queue_->AsyncAfter(delay, [this, execution_time](){
        this->MaybeExecuteTask(execution_time);
    });
}

void RepeatingTask::ExecuteTask() {
    if (this->handler_) {
        TimeDelta interval = this->handler_();
        if (interval.ms() > 0) {
            ScheduleTaskAfter(interval);
        } else {
            // Stop internally if the `interval` is not a positive number.
            this->is_stoped = true;
        }
    }
}
    
} // namespace naivertc
