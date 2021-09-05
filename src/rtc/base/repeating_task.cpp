#include "rtc/base/repeating_task.hpp"

namespace naivertc {

RepeatingTask::RepeatingTask(std::shared_ptr<Clock> clock, 
                             std::shared_ptr<TaskQueue> task_queue,
                            const TaskClouser clouser) 
    : clock_(clock),
      task_queue_(task_queue),
      clouser_(std::move(clouser)) {}

RepeatingTask::~RepeatingTask() {
    Stop();
}

void RepeatingTask::Start(TimeInterval delay_sec) {
    task_queue_->Async([&, delay_sec](){
        this->is_stoped = false;
        if (delay_sec == 0) {
            this->ExecuteTask();
        }else {
            this->ScheduleTaskAfter(delay_sec);
        }
    });
}

void RepeatingTask::Stop() {
    task_queue_->Async([&](){
        this->is_stoped = true;
    });
}

// Private methods
void RepeatingTask::ScheduleTaskAfter(TimeInterval delay_sec) {
    Timestamp execution_time = clock_->CurrentTime() + TimeDelta::Seconds(delay_sec);
    task_queue_->AsyncAfter(delay_sec, [&](){
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
        TimeInterval next_delay = this->clouser_();
        if (next_delay > 0) {
            ScheduleTaskAfter(next_delay);
        }
    }
}
    
} // namespace naivertc
