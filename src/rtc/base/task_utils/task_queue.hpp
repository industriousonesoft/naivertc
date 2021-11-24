#ifndef _RTC_BASE_TASK_UTILS_TASK_QUEUE_H_
#define _RTC_BASE_TASK_UTILS_TASK_QUEUE_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

#include <mutex>
#include <condition_variable>

namespace naivertc {

class RTC_CPP_EXPORT TaskQueue {
public:
    enum class Kind {
        BOOST,
        SIMULATED
    };
public:
    TaskQueue(std::string name, Kind kind = Kind::BOOST);
    TaskQueue(std::unique_ptr<TaskQueueImpl, TaskQueueImpl::Deleter> task_queue_impl);
    ~TaskQueue();

    void Async(std::function<void()> handler) const;
    void AsyncAfter(TimeDelta delay, std::function<void()> handler);
   
    void Sync(std::function<void()> handler) const;
    template<typename T>
    T Sync(std::function<T(void)> handler) const;

    bool IsCurrent() const;

private:
    TaskQueueImpl* const impl_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cond_;
};

template<typename T>
T TaskQueue::Sync(std::function<T(void)> handler) const {
    T ret;
    if (IsCurrent()) {
        ret = handler();
    } else {
        std::unique_lock<std::mutex> lock(mutex_);
        impl_->Post([this, handler=std::move(handler), &ret]{
            ret = handler();
            cond_.notify_one();
        });
        cond_.wait(lock);
    }
    return ret;
}

} // namespace naivertc

#endif