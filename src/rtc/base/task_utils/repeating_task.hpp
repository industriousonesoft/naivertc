#ifndef _RTC_BASE_TASK_UTILS_REPEATING_TASK_H_
#define _RTC_BASE_TASK_UTILS_REPEATING_TASK_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/task_utils/task_queue.hpp"

#include <plog/Log.h>

#include <memory>
#include <functional>

namespace naivertc {
class RTC_CPP_EXPORT RepeatingTask final {
public:
    using Handler = std::function<TimeDelta(void)>;
    static std::unique_ptr<RepeatingTask> DelayedStart(std::shared_ptr<Clock> clock, 
                                                       std::shared_ptr<TaskQueue> task_queue, 
                                                       TimeDelta delay, 
                                                       Handler clouser);
    static std::unique_ptr<RepeatingTask> Start(std::shared_ptr<Clock> clock, 
                                                std::shared_ptr<TaskQueue> task_queue,
                                                Handler clouser) {
        return RepeatingTask::DelayedStart(clock, task_queue, TimeDelta::Millis(0), std::move(clouser));
    }
public:
    ~RepeatingTask();

    // Stops future invocations of the repeating task.
    // The task is guaranteed to not be running after calling
    // this function, unless it is called from the clouser itself.
    // NOTE: Can only be called from the task queue where the task 
    // is running.
    void Stop();

    // Returns true untill Stop() was called.
    // NOTE: Can only be called from the task queue where the task 
    // is running.
    bool Running() const;
    
private:
    RepeatingTask(std::shared_ptr<Clock> clock, std::shared_ptr<TaskQueue> task_queue, Handler clouser);
    void Start(TimeDelta delay);
private:
    void ScheduleTaskAfter(TimeDelta delay);
    void MaybeExecuteTask(Timestamp execution_time);
    void ExecuteTask();
private:
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<TaskQueue> task_queue_;
    const Handler handler_;
    bool is_stoped_;
};
    
} // namespace naivertc


#endif