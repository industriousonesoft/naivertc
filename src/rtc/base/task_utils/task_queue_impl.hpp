#ifndef _RTC_BASE_TASK_UTILS_TASK_QUEUE_IMPL_H_
#define _RTC_BASE_TASK_UTILS_TASK_QUEUE_IMPL_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/synchronization/event.hpp"
#include "rtc/base/task_utils/queued_task.hpp"

#include <functional>

namespace naivertc {

class TaskQueueImpl {
public:
    struct Deleter {
        void operator()(TaskQueueImpl* task_queue) const { task_queue->Delete(); }
    };
public:
    virtual ~TaskQueueImpl() = default;
    // Starts destruction of the task queue.
    // On return ensures no task are running and no new tasks are 
    // able to start on the task queue.
    virtual void Delete() = 0;

    // Scheduls a closure to execute. Tasks are executed in FIFO order.
    virtual void Post(QueuedTask&& task) { RTC_NOTREACHED(); };
    // Scheduls a closure to execute a specified delay from when the call is made.
    virtual void PostDelayed(TimeDelta delay, QueuedTask&& task) { RTC_NOTREACHED(); };

    void Post(std::function<void()>&& handler) {
        Post(QueuedTask(std::move(handler)));
    }
    void PostDelayed(TimeDelta delay, std::function<void()>&& handler) {
        PostDelayed(delay, QueuedTask(std::move(handler)));
    }

    // Convenience method to invoke a functor on another thread, which
    // blocks the current thread until execution is complete.
    template<typename ReturnT,
             typename = typename std::enable_if<std::is_void<ReturnT>::value>::type>
    void Invoke(std::function<void()>&& handler) {
        if (IsCurrent()) {
            handler();
        } else {
            Post([this, handler=std::move(handler)]() {
                handler();
                event_.Set();
            });
            event_.WaitForever();
        }
    }
    template<typename ReturnT,
             typename = typename std::enable_if<!std::is_void<ReturnT>::value>::type>
    ReturnT Invoke(std::function<ReturnT()>&& handler) {
        ReturnT ret;
        if (IsCurrent()) {
            ret = handler();
        } else {
            Post([this, &ret, handler=std::move(handler)]() {
                ret = handler();
                event_.Set();
            });
            // FIXME: using mutex and condiction will block the caller thread sometimes, 
            // and i have no idea about this.
            event_.WaitForever();
        }
        return ret;
    }

    // Returns the task queue that is running the current thread.
    // Returns nullptr if this thread is not associated with any task queue.
    static TaskQueueImpl* Current();

    // Returns true if this task queue is running the current thread.
    bool IsCurrent() const { return Current() == this; }

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

private:
    mutable Event event_;
};

#define RTC_RUN_ON(x)   \
    assert((x)->IsCurrent() && "TaskQueue doesn't match.")
    
} // namespace naivertc


#endif