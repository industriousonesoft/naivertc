#ifndef _COMMON_TASK_QUEUE_H_
#define _COMMON_TASK_QUEUE_H_

#include "base/defines.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/thread/thread.hpp>

#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT TaskQueue {
public:
    TaskQueue();
    ~TaskQueue();

    void Post(std::function<void()> f) const;
    void Dispatch(std::function<void()> f) const;

    bool is_in_current_queue() const;

private:
    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    boost::asio::io_context::strand strand_;
    std::unique_ptr<boost::thread> ioc_thread_;
};

}

#endif