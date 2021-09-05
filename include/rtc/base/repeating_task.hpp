#ifndef _RTC_BASE_REPEATING_TASK_H_
#define _RTC_BASE_REPEATING_TASK_H_

#include "base/defines.hpp"
#include "rtc/base/clock.hpp"
#include "common/task_queue.hpp"

#include <plog/Log.h>

#include <memory>
#include <functional>

namespace naivertc {
// TODO: Repeating task unittest!!!
class RTC_CPP_EXPORT RepeatingTask {
public:
    using TaskClouser = std::function<TimeInterval(void)>;
public:
    RepeatingTask(std::shared_ptr<Clock> clock, std::shared_ptr<TaskQueue> task_queue, const TaskClouser clouser);
    ~RepeatingTask();

    void Start(TimeInterval delay_sec);
    void Stop();

private:
    void ScheduleTaskAfter(TimeInterval delay_sec);
    void MaybeExecuteTask(Timestamp execution_time);
    void ExecuteTask();
private:
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<TaskQueue> task_queue_;
    const TaskClouser clouser_;
    bool is_stoped = true;
};
    
} // namespace naivertc


#endif