#ifndef _RTC_BASE_TASK_UTILS_TASK_QUEUE_H_
#define _RTC_BASE_TASK_UTILS_TASK_QUEUE_H_

#include "base/defines.hpp"
#include "common/thread_utils.hpp"
#include "rtc/base/units/time_delta.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/thread/thread.hpp>

#include <functional>
#include <list>
#include <thread>

namespace naivertc {

class RTC_CPP_EXPORT TaskQueue {
public:
    TaskQueue(std::string name);
    ~TaskQueue();

    void Async(std::function<void()> handler) const;
    void AsyncAfter(TimeDelta delay, std::function<void()> handler);
   
    void Sync(std::function<void()> handler) const;
    template<typename T>
    T Sync(std::function<T(void)> handler) const {
        T ret;
        if (IsCurrent()) {
            ret = handler();
        } else {
            boost::unique_lock<boost::mutex> lock(mutex_);
            boost::asio::dispatch(strand_, [this, handler = std::move(handler), &ret](){
                ret = handler();
                cond_.notify_one();
            });
            cond_.wait(lock);
        }
        return ret;
    }

    bool IsCurrent() const;

private:
    void ScheduleTaskAfter(TimeDelta delay, std::function<void()> handler);

private:
    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    boost::asio::io_context::strand strand_;
    std::unique_ptr<boost::thread> ioc_thread_;
    // std::unique_ptr<std::thread> ioc_thread_;
    // PlatformThreadId task_queue_thread_id_;

    std::list<boost::asio::deadline_timer*> pending_timers_;

    mutable boost::mutex mutex_;
    mutable boost::condition_variable cond_;
};

}

#endif