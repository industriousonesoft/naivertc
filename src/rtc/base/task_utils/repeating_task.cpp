#include "rtc/base/task_utils/repeating_task.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

namespace naivertc {

std::unique_ptr<RepeatingTask> RepeatingTask::DelayedStart(Clock* clock,
                                                           TaskQueueImpl* task_queue,
                                                           TimeDelta delay, 
                                                           Clouser&& closure) {
    auto safety_flag = PendingTaskSafetyFlag::CreateDetached();
    auto task = std::unique_ptr<RepeatingTask>(new RepeatingTask(clock, 
                                                                 task_queue, 
                                                                 std::move(closure),
                                                                 std::move(safety_flag)));
    task->Start(delay);
    return task;
}

RepeatingTask::RepeatingTask(Clock* clock,
                             TaskQueueImpl* task_queue,
                             Clouser&& closure,
                             std::shared_ptr<PendingTaskSafetyFlag> safety_flag) 
    : clock_(clock),
      task_queue_(task_queue),
      closure_(std::move(closure)),
      safety_flag_(std::move(safety_flag)) {
    assert(task_queue != nullptr);
    assert(safety_flag_ != nullptr);
}

RepeatingTask::~RepeatingTask() {
    Stop();
};

void RepeatingTask::Start(TimeDelta delay) {
    task_queue_->Post(ToQueuedTask(safety_flag_, [this, delay](){
        if (delay.ms() <= 0) {
            this->ExecuteTask();
        } else {
            this->ScheduleTaskAfter(delay);
        }
    }));
}

bool RepeatingTask::Running() const {
    return task_queue_->Invoke<bool>([this](){
        return safety_flag_->alive();
    });
}

void RepeatingTask::Stop() {
    task_queue_->Invoke<void>([this](){
        safety_flag_->SetNotAlive();
    });
}

// Private methods
void RepeatingTask::ScheduleTaskAfter(TimeDelta delay) {
    RTC_RUN_ON(task_queue_);
    Timestamp execution_time = clock_->CurrentTime() + delay;
    task_queue_->PostDelayed(delay, ToQueuedTask(safety_flag_, [this, execution_time](){
        MaybeExecuteTask(execution_time);
    }));
}

void RepeatingTask::MaybeExecuteTask(Timestamp execution_time) {
    RTC_RUN_ON(task_queue_);
    Timestamp now = clock_->CurrentTime();
    if (now >= execution_time) {
        ExecuteTask();
        return;
    }

    PLOG_WARNING << "RepeatingTask: scheduled delayed called too early.";
    TimeDelta delay = execution_time - now;
    task_queue_->PostDelayed(delay, ToQueuedTask(safety_flag_, [this, execution_time](){
        MaybeExecuteTask(execution_time);
    }));
}

void RepeatingTask::ExecuteTask() {
    RTC_RUN_ON(task_queue_);
    TimeDelta interval = closure_();
    if (interval.ms() > 0) {
        ScheduleTaskAfter(interval);
    } else {
        // Stop internally if the `interval` is not a positive number.
        safety_flag_->SetNotAlive();
    }
}
    
} // namespace naivertc
