#include "common/task_queue.hpp"

#include <plog/Log.h>

#include <mutex>

namespace naivertc {

// std::shared_ptr<TaskQueue> TaskQueue::GlobalTaskQueue = std::make_shared<TaskQueue>();

TaskQueue::TaskQueue(const std::string name) 
    : work_guard_(boost::asio::make_work_guard(ioc_)),
      strand_(ioc_),
      timer_(ioc_) {
    // The thread will start immediately after created
    ioc_thread_.reset(new boost::thread(boost::bind(&boost::asio::io_context::run, &ioc_)));
    if (!name.empty()) {
        // TODO: Set a name for thread
        PLOG_VERBOSE << "Task queue name: " << name << " in thread: " << ioc_thread_->get_id();
    }
}

TaskQueue::~TaskQueue() {

    PLOG_VERBOSE << __FUNCTION__;

    // Indicate that the work is no longer working, ioc will exit later.
    work_guard_.reset();
    if (ioc_.stopped()) {
        PLOG_VERBOSE << "io_context of task queue exited";
    }
    // It is considered an error to desctory a C++ thread object while it is
    // still joinable. That is, in order to desctory a C++ threa object either join() needs to be
    // called (and completed) or detach() must be called. If a C++ thread object is still joinable when
    // it is destroyed, an exception will be thrown.
    // See https://stackoverflow.com/questions/37015775/what-is-different-between-join-and-detach-for-multi-threading-in-c
    if (ioc_thread_->joinable()) {
        // The thread::join() is called, the calling thread will block until
        // the thread of execution has completed. Basically, this is one mechainism 
        // that can be used to know when a thread has finished. When thread::join() 
        // returns, the thread object can be destroyed.
        PLOG_VERBOSE << "Join task queue";
        ioc_thread_->join();
    }
    ioc_thread_.reset();
}

void TaskQueue::Sync(const std::function<void()> handler) const {
    if (is_in_current_queue()) {
        handler();
    }else {
        boost::unique_lock<boost::mutex> lock(mutex_);
        boost::asio::dispatch(strand_, [this, handler = std::move(handler)](){
            handler();
            cond_.notify_one();
        });
        cond_.wait(lock);
    }
}

void TaskQueue::Async(const std::function<void()> handler) const {
    if (is_in_current_queue()) {
        handler();
    }else {
        boost::asio::post(strand_, std::move(handler));
    }
}

void TaskQueue::Dispatch(const std::function<void()> handler) const {
    if (is_in_current_queue()) {
        handler();
    }else {
        boost::asio::dispatch(strand_, std::move(handler));
    }
}

void TaskQueue::AsyncAfter(TimeInterval delay_in_sec, const std::function<void()> handler) {
    // Construct a timer without setting an expiry time.
    timer_.expires_from_now(boost::posix_time::seconds(delay_in_sec));
    // Start an asynchronous wait
    timer_.async_wait([handler = std::move(handler)](const boost::system::error_code& error){
         handler();
    });
}

bool TaskQueue::is_in_current_queue() const {
    // NOTE: DO NOT call get_id() in a detached thread, it will return 'Not-any-thread'
    return ioc_thread_->get_id() == boost::this_thread::get_id();    
}


}