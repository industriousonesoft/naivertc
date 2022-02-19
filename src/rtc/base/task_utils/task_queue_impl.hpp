#ifndef _RTC_BASE_TASK_UTILS_TASK_QUEUE_IMPL_H_
#define _RTC_BASE_TASK_UTILS_TASK_QUEUE_IMPL_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"

#include <functional>

namespace naivertc {

class TaskQueueImpl {
public:
    struct Deleter {
        void operator()(TaskQueueImpl* task_queue) const { task_queue->Delete(); }
    };
public:
    // Starts destruction of the task queue.
    // On return ensures no task are running and no new tasks are 
    // able to start on the task queue.
    virtual void Delete() = 0;

    // Scheduls a task to execute. Tasks are executed in FIFO order.
    virtual void Post(std::function<void()> handler) = 0;

    // Scheduls a task to execute a specified delay from when the call is made.
    virtual void PostDelayed(TimeDelta delay, std::function<void()> handler) = 0;

    // Returns the task queue that is running the current thread.
    // Returns nullptr if this thread is not associated with any task queue.
    static TaskQueueImpl* Current();

    // Returns true if this task queue is running the current thread.
    bool IsCurrent() const { return Current() == this; }

protected:
    // Users of the TaskQueue should call Delete instead of 
    // directly deleting this instance.
    virtual ~TaskQueueImpl() = default;

protected:
    class CurrentTaskQueueSetter {
    public:
        explicit CurrentTaskQueueSetter(TaskQueueImpl* task_queue);
        CurrentTaskQueueSetter(const CurrentTaskQueueSetter&) = delete;
        CurrentTaskQueueSetter& operator=(const CurrentTaskQueueSetter&) = delete;
        ~CurrentTaskQueueSetter();
    private:
        TaskQueueImpl* const previous_;
    };
};

#define RTC_RUN_ON(x)   \
    assert((x)->IsCurrent() && "TaskQueue doesn't match.")
    
} // namespace naivertc


#endif