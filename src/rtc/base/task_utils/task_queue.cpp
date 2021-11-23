#include "rtc/base/task_utils/task_queue.hpp"

#include <plog/Log.h>

namespace naivertc {

TaskQueue::TaskQueue(std::string name) 
    : work_guard_(boost::asio::make_work_guard(ioc_)),
      strand_(ioc_) {
    // The thread will start immediately after created
    // ioc_thread_.reset(new boost::thread(boost::bind(&boost::asio::io_context::run, &ioc_)));
    ioc_thread_.reset(new boost::thread([this, name=std::move(name)](){
        if (!name.empty()) {
            // FIXME: This seems not working?
            SetCurrentThreadName(name.c_str());
        }
        // task_queue_thread_id_ = CurrentThreadId();
        ioc_.run();
        PLOG_VERBOSE << "ioc_thread of task queue exited.";
    }));
}

TaskQueue::~TaskQueue() {

    PLOG_VERBOSE << __FUNCTION__ << " will destroy.";

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
        PLOG_VERBOSE << "Blocking thread and waiting all task done.";
        ioc_thread_->join();
    }
    ioc_thread_.reset();
    PLOG_VERBOSE << __FUNCTION__ << " did destroy.";
}

void TaskQueue::Sync(std::function<void()> handler) const {
    if (IsCurrent()) {
        handler();
    } else {
        boost::unique_lock<boost::mutex> lock(mutex_);
        boost::asio::dispatch(strand_, [this, handler = std::move(handler)](){
            handler();
            cond_.notify_one();
        });
        cond_.wait(lock);
    }
}

void TaskQueue::Async(std::function<void()> handler) const {
    if (IsCurrent()) {
        handler();
    } else {
        boost::asio::post(strand_, std::move(handler));
    }
}

void TaskQueue::Dispatch(std::function<void()> handler) const {
    if (IsCurrent()) {
        handler();
    } else {
        boost::asio::dispatch(strand_, std::move(handler));
    }
}

void TaskQueue::AsyncAfter(TimeInterval delay_in_sec, std::function<void()> handler) {
     if (IsCurrent()) {
        // Construct a timer without setting an expiry time.
        boost::asio::deadline_timer* timer = new boost::asio::deadline_timer(ioc_, boost::posix_time::seconds(delay_in_sec));
        // Start an asynchronous wait
        timer->async_wait([this, timer, handler = std::move(handler)](const boost::system::error_code& error){
            handler();
            pending_timers_.remove(timer);
        });
        pending_timers_.push_back(timer);
    } else {
        boost::asio::post(strand_, [this, delay_in_sec, handler = std::move(handler)](){
            AsyncAfter(delay_in_sec, std::move(handler));
        });
    }
}

bool TaskQueue::IsCurrent() const {
    // NOTE: DO NOT call get_id() in a detached thread, it will return 'Not-any-thread'
    return ioc_thread_->get_id() == boost::this_thread::get_id();    
    // return task_queue_thread_id_ == CurrentThreadId();
}


}