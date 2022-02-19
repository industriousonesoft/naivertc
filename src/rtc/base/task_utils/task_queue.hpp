#ifndef _RTC_BASE_TASK_UTILS_TASK_QUEUE_H_
#define _RTC_BASE_TASK_UTILS_TASK_QUEUE_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

#define SUPPORT_YIELD
#if defined(SUPPORT_YIELD)
#include "rtc/base/synchronization/event.hpp"
#else
#include <mutex>
#include <condition_variable>
#endif

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

    void Post(std::function<void()> handler) const;
    void PostDelayed(TimeDelta delay, std::function<void()> handler) const;
  
    // Convenience method to invoke a functor on another thread, which
    // blocks the current thread until execution is complete.
    template<typename ReturnT,
             typename = typename std::enable_if<std::is_void<ReturnT>::value>::type>
    void Invoke(std::function<void()> handler) const {
        InvokeInternal(std::move(handler));
    }

    template<typename ReturnT,
             typename = typename std::enable_if<!std::is_void<ReturnT>::value>::type>
    ReturnT Invoke(std::function<ReturnT()> handler) const {
        ReturnT ret;
        InvokeInternal([&ret, handler=std::move(handler)](){
            ret = handler();
        });
        return ret;
    }

    bool IsCurrent() const;

    // Returns non-owning pointer to the task queue implementation.
    TaskQueueImpl* Get() { return impl_; }

private:
    void InvokeInternal(std::function<void()> handler) const;

private:
    TaskQueueImpl* const impl_;
#if defined(SUPPORT_YIELD)
    // NOTE: Using Event instead of std::mutex for YieldPolicy tests.
    mutable Event event_;
#else
    mutable std::mutex mutex_;
    mutable std::condition_variable cond_;
#endif
};

} // namespace naivertc

#endif