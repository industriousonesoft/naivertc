#include "rtc/base/repeating_task.hpp"

namespace naivertc {

std::unique_ptr<RepeatingTask> RepeatingTask::DelayedStart(std::shared_ptr<Clock> clock, 
                                                           std::shared_ptr<TaskQueue> task_queue, 
                                                           TimeDelta delay, 
                                                           const TaskClouser clouser) {
    auto task = std::unique_ptr<RepeatingTask>(new RepeatingTask(clock, task_queue, clouser));
    task->Start(delay);
    return task;
}

RepeatingTask::RepeatingTask(std::shared_ptr<Clock> clock, 
                             std::shared_ptr<TaskQueue> task_queue,
                             const TaskClouser clouser) 
    : clock_(clock),
      task_queue_(task_queue != nullptr ? task_queue 
                                        : std::make_shared<TaskQueue>("RepeatingTask.default.task.queue")),
      clouser_(std::move(clouser)) {}

RepeatingTask::~RepeatingTask() {
    Stop();
}

void RepeatingTask::Start(TimeDelta delay) {
    task_queue_->Async([this, delay=std::move(delay)](){
        this->is_stoped = false;
        if (delay.IsZero()) {
            this->ExecuteTask();
        } else {
            this->ScheduleTaskAfter(delay);
        }
    });
}

void RepeatingTask::Stop() {
    task_queue_->Async([&](){
        this->is_stoped = true;
    });
}

// Private methods
void RepeatingTask::ScheduleTaskAfter(TimeDelta delay) {
    Timestamp execution_time = clock_->CurrentTime() + delay;
    task_queue_->AsyncAfter(delay.seconds(), [&](){
        this->MaybeExecuteTask(execution_time);
    });
}

void RepeatingTask::MaybeExecuteTask(Timestamp execution_time) {
    Timestamp now = clock_->CurrentTime();
    if (now >= execution_time) {
        ExecuteTask();
        return;
    }

    PLOG_WARNING << "RepeatingTask: scheduled delayed called too early.";

    TimeDelta delay = execution_time - now;
    task_queue_->AsyncAfter(delay.seconds(), [this, execution_time](){
        this->MaybeExecuteTask(execution_time);
    });
}

void RepeatingTask::ExecuteTask() {
    if (this->clouser_ && !this->is_stoped) {
        TimeDelta interval = this->clouser_();
        if (!interval.IsZero()) {
            ScheduleTaskAfter(interval);
        }
    }
}
    
} // namespace naivertc
