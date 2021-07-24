#ifndef _COMMON_TASK_QUEUE_H_
#define _COMMON_TASK_QUEUE_H_

#include "base/defines.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/thread/thread.hpp>

#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT TaskQueue : std::enable_shared_from_this<TaskQueue> {
public:
    TaskQueue();
    ~TaskQueue();

    void Sync(const std::function<void(void)> handler) const;
    void Async(const std::function<void()> handler) const;
    void AsyncAfter(TimeInterval delay_in_sec, const std::function<void()> handler);

    template<typename T>
    T Sync(std::function<T(void)> handler) const {
        T ret;
        if (is_in_current_queue()) {
            ret = handler();
        }else {
            boost::unique_lock<boost::mutex> lock(mutex_);
            boost::asio::dispatch(strand_, [this, handler = std::move(handler), &ret](){
                ret = handler();
                cond_.notify_one();
            });
            cond_.wait(lock);
        }
        return ret;
    }

    void Dispatch(const std::function<void()> handler) const;

    bool is_in_current_queue() const;

private:
    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    boost::asio::io_context::strand strand_;
    std::unique_ptr<boost::thread> ioc_thread_;
    boost::asio::deadline_timer timer_;

    mutable boost::mutex mutex_;
    mutable boost::condition_variable cond_;

};

}

#endif