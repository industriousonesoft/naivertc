#ifndef _TASK_QUEUE_TESTS_H_
#define _TASK_QUEUE_TESTS_H_

#include <common/task_queue.hpp>
#include <rtc/base/repeating_task.hpp>
#include <rtc/base/clock_real_time.hpp>

namespace taskqueue {

class Example {
public:
    Example();
    ~Example();

    void DelayPost();
    void Post();
    void RepeatingTask(TimeInterval delay);

private:
    std::shared_ptr<naivertc::RealTimeClock> clock_;
    std::shared_ptr<naivertc::TaskQueue> task_queue_;
    std::shared_ptr<naivertc::RepeatingTask> repeating_task_;
    naivertc::Timestamp last_execution_time_;

};

} // namespace taskqueue


#endif