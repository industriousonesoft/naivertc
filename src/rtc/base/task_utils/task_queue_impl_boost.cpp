#include "rtc/base/task_utils/task_queue_impl_boost.hpp"
#include "common/utils_time.hpp"
#include "common/thread_utils.hpp"

#include <plog/Log.h>

namespace naivertc {

std::unique_ptr<TaskQueueImpl, TaskQueueImpl::Deleter> TaskQueueImplBoost::Create(std::string name) {
    return std::unique_ptr<TaskQueueImpl, TaskQueueImpl::Deleter>(new TaskQueueImplBoost(std::move(name)));
}

TaskQueueImplBoost::TaskQueueImplBoost(std::string name) 
    : work_guard_(boost::asio::make_work_guard(ioc_)),
      strand_(ioc_) {
    // The thread will start immediately after created
    // ioc_thread_.reset(new boost::thread(boost::bind(&boost::asio::io_context::run, &ioc_)));
    ioc_thread_.reset(new boost::thread([this, name=std::move(name)](){
        if (!name.empty()) {
            // FIXME: This seems not working?
            SetCurrentThreadName(name.c_str());
        }
        // Set the current task queue of the thread.
        CurrentTaskQueueSetter set_current(this);
        // Run and block the thread.
        ioc_.run();
        PLOG_VERBOSE << "ioc_thread of task queue exited.";
    }));
}

TaskQueueImplBoost::~TaskQueueImplBoost() = default;

void TaskQueueImplBoost::Delete() {
    assert(IsCurrent() == false);
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
    // Delete itself after the associated thread exited.
    delete this;
}

void TaskQueueImplBoost::Post(QueuedTask&& task) {
    boost::asio::post(strand_, std::move(task));
}

void TaskQueueImplBoost::PostDelayed(TimeDelta delay, QueuedTask&& task) {
    if (IsCurrent()) {
        if (delay.ms() > 0) {
            ScheduleTaskAfter(delay, std::move(task));
        } else {
            boost::asio::post(strand_, std::move(task));
        }
    } else {
        uint32_t posted_time_ms = utils::time::Time32InMillis();
        boost::asio::post(strand_, [this, delay, posted_time_ms, task = std::move(task)]() mutable {
            uint32_t elasped_ms = utils::time::Time32InMillis() - posted_time_ms;
            if (delay.ms() > elasped_ms) {
                ScheduleTaskAfter(delay - TimeDelta::Millis(elasped_ms), std::move(task));
            } else {
                boost::asio::post(strand_, std::move(task));
            }
        });
    }
}

// Private methods
void TaskQueueImplBoost::ScheduleTaskAfter(TimeDelta delay, QueuedTask&& task) {
    assert(IsCurrent());
    // Construct a timer without setting an expiry time.
    boost::asio::deadline_timer* timer = new boost::asio::deadline_timer(ioc_, boost::posix_time::milliseconds(delay.ms()));
    // Start an asynchronous wait
    timer->async_wait([this, timer, task = std::move(task)](const boost::system::error_code& error) mutable {
        task();
        pending_timers_.remove(timer);
    });
    pending_timers_.push_back(timer);
}
    
} // namespace naivert 
