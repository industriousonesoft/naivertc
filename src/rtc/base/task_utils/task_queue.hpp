#ifndef _RTC_BASE_TASK_QUEUE_H_
#define _RTC_BASE_TASK_QUEUE_H_

#include "base/defines.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/thread/thread.hpp>

#include <functional>
#include <list>

namespace naivertc {

class RTC_CPP_EXPORT TaskQueue {
public:
    TaskQueue(std::string&& name = "");
    virtual ~TaskQueue();

    virtual void Sync(std::function<void()> handler) const;
    virtual void Async(std::function<void()> handler) const;
    virtual void AsyncAfter(TimeInterval delay_in_sec, std::function<void()> handler);

    template<typename T>
    T Sync(std::function<T(void)> handler) const {
        T ret;
        if (is_in_current_queue()) {
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

    virtual void Dispatch(std::function<void()> handler) const;

    bool is_in_current_queue() const;

private:
    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    boost::asio::io_context::strand strand_;
    std::unique_ptr<boost::thread> ioc_thread_;
    std::list<boost::asio::deadline_timer*> pending_timers_;

    mutable boost::mutex mutex_;
    mutable boost::condition_variable cond_;
};

}

#endif