#ifndef _TASK_QUEUE_TESTS_H_
#define _TASK_QUEUE_TESTS_H_

#include <common/task_queue.hpp>
#include <rtc/base/repeating_task.hpp>
#include <rtc/base/clock_real_time.hpp>

namespace taskqueue {

using namespace naivertc;

class Example {
public:
    Example();
    ~Example();

    void DelayPost();
    void Post();
    void TestRepeatingTask();

private:
    std::shared_ptr<RealTimeClock> clock_;
    std::shared_ptr<TaskQueue> task_queue_;
    std::shared_ptr<RepeatingTask> repeating_task_;
    Timestamp last_execution_time_;
};

} // namespace taskqueue


#endif