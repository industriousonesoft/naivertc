#ifndef _RTC_BASE_TASK_UTILS_REPEATING_TASK_H_
#define _RTC_BASE_TASK_UTILS_REPEATING_TASK_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/task_utils/queued_task.hpp"

#include <plog/Log.h>

#include <memory>
#include <functional>

namespace naivertc {

class TaskQueueImpl;

class RepeatingTask final {
public:
    using Clouser = std::function<TimeDelta(void)>;
    static std::unique_ptr<RepeatingTask> DelayedStart(Clock* clock,
                                                       TaskQueueImpl* task_queue,
                                                       TimeDelta delay,
                                                       Clouser&& closure);
    static std::unique_ptr<RepeatingTask> Start(Clock* clock,
                                                TaskQueueImpl* task_queue,
                                                Clouser&& closure) {
        return RepeatingTask::DelayedStart(clock, task_queue, TimeDelta::Millis(0), std::move(closure));
    }
public:
    ~RepeatingTask();

    // The future invocations of the repeating task is guaranteed to not be 
    // running after calling this function, unless this function is called from 
    // the clouser itself.
    void Stop();

    // Returns true untill Stop() was called.
    bool Running() const;
    
private:
    RepeatingTask(Clock* clock, 
                  TaskQueueImpl* task_queue, 
                  Clouser&& closure, 
                  std::shared_ptr<PendingTaskSafetyFlag> safety_flag);
    void Start(TimeDelta delay);
private:
    void ScheduleTaskAfter(TimeDelta delay);
    void MaybeExecuteTask(Timestamp execution_time);
    void ExecuteTask();
private:
    Clock* const clock_;
    TaskQueueImpl* const task_queue_;
    const Clouser closure_;
    std::shared_ptr<PendingTaskSafetyFlag> safety_flag_;
};
    
} // namespace naivertc


#endif