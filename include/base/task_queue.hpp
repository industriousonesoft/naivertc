#ifndef _BASE_TASK_QUEUE_H_
#define _BASE_TASK_QUEUE_H_

#include "base/defines.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/thread/thread.hpp>

namespace naivertc {

class RTC_CPP_EXPORT TaskQueue {
public:
    TaskQueue();
    ~TaskQueue();

    template<typename Function>
    void Post(Function && f) const;

    template<typename Function>
    void Dispatch(Function && f) const;

private:
    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    boost::asio::io_context::strand strand_;
    std::unique_ptr<boost::thread> ioc_thread_;
};

}

#endif