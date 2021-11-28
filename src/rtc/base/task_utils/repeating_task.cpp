#include "rtc/base/task_utils/repeating_task.hpp"

namespace naivertc {

std::unique_ptr<RepeatingTask> RepeatingTask::DelayedStart(std::shared_ptr<Clock> clock, 
                                                           std::shared_ptr<TaskQueue> task_queue, 
                                                           TimeDelta delay, 
                                                           Handler handler) {
    auto task = std::unique_ptr<RepeatingTask>(new RepeatingTask(clock, task_queue, std::move(handler)));
    task->Start(delay);
    return task;
}

RepeatingTask::RepeatingTask(std::shared_ptr<Clock> clock, 
                             std::shared_ptr<TaskQueue> task_queue,
                             Handler handler) 
    : clock_(clock),
      task_queue_(task_queue != nullptr ? std::move(task_queue) 
                                        : std::make_shared<TaskQueue>("RepeatingTask.default.task.queue")),
      handler_(std::move(handler)),
      is_stoped_(true) {}

RepeatingTask::~RepeatingTask() = default;

void RepeatingTask::Start(TimeDelta delay) {
    task_queue_->Async([this, delay=std::move(delay)](){
        this->is_stoped_ = false;
        if (delay.ms() <= 0) {
            this->ExecuteTask();
        } else {
            this->ScheduleTaskAfter(delay);
        }
    });
}

bool RepeatingTask::Running() const {
    assert(task_queue_->IsCurrent());
    return !is_stoped_;
}

void RepeatingTask::Stop() {
    assert(task_queue_->IsCurrent());
    is_stoped_ = true;
}

// Private methods
void RepeatingTask::ScheduleTaskAfter(TimeDelta delay) {
    Timestamp execution_time = clock_->CurrentTime() + delay;
    task_queue_->AsyncAfter(delay, [this, execution_time](){
        this->MaybeExecuteTask(execution_time);
    });
}

void RepeatingTask::MaybeExecuteTask(Timestamp execution_time) {
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
            this->is_stoped_ = true;
        }
    }
}
    
} // namespace naivertc
