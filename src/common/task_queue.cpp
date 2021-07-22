#include "common/task_queue.hpp"

#include <plog/Log.h>

#include <mutex>

namespace naivertc {

std::shared_ptr<TaskQueue> TaskQueue::GlobalTaskQueue = std::make_shared<TaskQueue>();

TaskQueue::TaskQueue() 
    : work_guard_(boost::asio::make_work_guard(ioc_)),
      strand_(ioc_),
      timer_(ioc_) {
    ioc_thread_.reset(new boost::thread(boost::bind(&boost::asio::io_context::run, &ioc_)));
    ioc_thread_id_ = ioc_thread_->get_id();
    // FIXME: 不能再自身线程亦或是在detach之后不能调用get_id()，否则返回值为{Not-any-thread}，Why?
    ioc_thread_->detach();
}

TaskQueue::~TaskQueue() {
    ioc_.stop();
    // ioc_thread_ will exist after ioc stoped.
    ioc_thread_.reset();
}

void TaskQueue::Post(const std::function<void()>& handler) const {
    if (is_in_current_queue()) {
        handler();
    }else {
        boost::asio::post(strand_, handler);
    }
}

void TaskQueue::Dispatch(const std::function<void()>& handler) const {
    if (is_in_current_queue()) {
        handler();
    }else {
        boost::asio::dispatch(strand_, handler);
    }
}

void TaskQueue::PostDelay(TimeInterval delay_in_sec, const std::function<void()>& handler) {
    // Construct a timer without setting an expiry time.
    timer_.expires_from_now(boost::posix_time::seconds(delay_in_sec));
    // Start an asynchronous wait
    timer_.async_wait([&handler](const boost::system::error_code& error){
         handler();
    });
}

bool TaskQueue::is_in_current_queue() const {
    return ioc_thread_id_ == boost::this_thread::get_id();    
}

void TaskQueue::PostInGlobalQueue(std::function<void()> handler) {
    GlobalTaskQueue->Post(std::move(handler));
}

void TaskQueue::DispatchInGlobalQueue(std::function<void()> handler) {
    GlobalTaskQueue->Dispatch(std::move(handler));
}

void TaskQueue::PostDelayInGlobalQueue(TimeInterval delay_in_sec ,std::function<void()> handler) {
    GlobalTaskQueue->PostDelay(delay_in_sec, std::move(handler));
}

}