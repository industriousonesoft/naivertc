#include "base/task_queue.hpp"

#include <boost/asio.hpp>

namespace naivertc {

TaskQueue::TaskQueue() 
    : work_guard_(boost::asio::make_work_guard(ioc_)),
      strand_(ioc_) {
    ioc_thread_.reset(new boost::thread(boost::bind(&boost::asio::io_context::run, &ioc_)));
    ioc_thread_->detach();
}

TaskQueue::~TaskQueue() {
    ioc_.stop();
    // ioc_thread_ will exist after ioc stoped.
    ioc_thread_.reset();
}

void TaskQueue::Post(std::function<void()> f) const {
    boost::asio::post(strand_, [f](){
        f();
    });
}

void TaskQueue::Dispatch(std::function<void()> f) const {
    boost::asio::dispatch(strand_, f);
}

}