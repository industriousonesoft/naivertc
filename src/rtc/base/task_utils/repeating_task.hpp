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
    using TaskHandler = std::function<TimeDelta(void)>;
    static std::unique_ptr<RepeatingTask> DelayedStart(std::shared_ptr<Clock> clock, 
                                                       std::shared_ptr<TaskQueue> task_queue, 
                                                       TimeDelta delay, 
                                                       TaskHandler clouser);
    static std::unique_ptr<RepeatingTask> Start(std::shared_ptr<Clock> clock, 
                                                std::shared_ptr<TaskQueue> task_queue,
                                                TaskHandler clouser) {
        return RepeatingTask::DelayedStart(clock, task_queue, TimeDelta::Seconds(0), std::move(clouser));
    }
public:
    ~RepeatingTask();

    bool Running() const;
    void Stop();
private:
    RepeatingTask(std::shared_ptr<Clock> clock, std::shared_ptr<TaskQueue> task_queue, TaskHandler clouser);
    void Start(TimeDelta delay);
private:
    void ScheduleTaskAfter(TimeDelta delay);
    void MaybeExecuteTask(Timestamp execution_time);
    void ExecuteTask();
private:
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<TaskQueue> task_queue_;
    const TaskHandler handler_;
    bool is_stoped = true;
};
    
} // namespace naivertc


#endif