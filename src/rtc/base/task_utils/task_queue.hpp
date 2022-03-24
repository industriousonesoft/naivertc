#ifndef _RTC_BASE_TASK_UTILS_TASK_QUEUE_H_
#define _RTC_BASE_TASK_UTILS_TASK_QUEUE_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

namespace naivertc {

class TaskQueue {
public:
    enum class Kind {
        BOOST
    };
public:
    TaskQueue(std::string name, Kind kind = Kind::BOOST);
    TaskQueue(std::unique_ptr<TaskQueueImpl, TaskQueueImpl::Deleter> task_queue_impl);
    ~TaskQueue();

    void Post(std::unique_ptr<QueuedTask> task);
    void PostDelayed(TimeDelta delay, std::unique_ptr<QueuedTask> task);

    template<typename Closure,
             typename std::enable_if<!std::is_convertible<
                Closure,
                std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
    void Post(Closure&& closure) {
        Post(ToQueuedTask(std::forward<Closure>(closure)));
    }
    template<typename Closure,
             typename std::enable_if<!std::is_convertible<
                Closure,
                std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
    void PostDelayed(TimeDelta delay, Closure&& closure) {
        PostDelayed(delay, ToQueuedTask(std::forward<Closure>(closure)));
    }

    // Convenience method to invoke a functor on another thread, which
    // blocks the current thread until execution is complete.
    template<typename ReturnT,
             typename = typename std::enable_if<std::is_void<ReturnT>::value>::type>
    void Invoke(std::function<void()>&& handler) {
        impl_->Invoke<void>(std::move(handler));
    }
    template<typename ReturnT,
             typename = typename std::enable_if<!std::is_void<ReturnT>::value>::type>
    ReturnT Invoke(std::function<ReturnT()>&& handler) {
        return impl_->Invoke<ReturnT>(std::move(handler));
    }

    bool IsCurrent() const;

    // Returns non-owning pointer to the task queue implementation.
    TaskQueueImpl* Get() { return impl_; }

private:
    TaskQueueImpl* const impl_;
};

} // namespace naivertc

#endif