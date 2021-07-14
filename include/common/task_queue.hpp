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

    void Post(std::function<void()> handler) const;
    void Dispatch(std::function<void()> handler) const;
    void PostDelay(TimeInterval delay_in_sec ,std::function<void()> handler);

    bool is_in_current_queue() const;

    template<typename T>
    T SyncPost(std::function<T(void)> handler) const {
        boost::unique_lock<boost::mutex> lock(mutex_);
        T ret;
        Post([this, handler = std::move(handler), &ret](){
            ret = handler();
            cond_.notify_one();
        });
        cond_.wait(lock);
        return ret;
    }

public:
    static void PostInGlobalQueue(std::function<void()> handler);
    static void DispatchInGlobalQueue(std::function<void()> handler);
    static void PostDelayInGlobalQueue(TimeInterval delay_in_sec ,std::function<void()> handler);

private:
    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    boost::asio::io_context::strand strand_;
    std::unique_ptr<boost::thread> ioc_thread_;
    boost::thread::id ioc_thread_id_;
    boost::asio::deadline_timer timer_;

    mutable boost::mutex mutex_;
    mutable boost::condition_variable cond_;

    static std::shared_ptr<TaskQueue> GlobalTaskQueue;
};

}

#endif