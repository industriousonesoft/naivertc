#include "rtc/base/task_utils/task_queue.hpp"
#include "common/utils_time.hpp"
#include "common/thread_utils.hpp"
#include "rtc/base/task_utils/task_queue_impl_boost.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

std::unique_ptr<TaskQueueImpl, TaskQueueImpl::Deleter> CreateTaskQueue(std::string name, TaskQueue::Kind kind) {
    switch (kind) {
    case TaskQueue::Kind::BOOST:
        return TaskQueueImplBoost::Create(std::move(name));
    case TaskQueue::Kind::SIMULATED:
        return nullptr;
    default:
        return nullptr;
    }
}

} // namespac 

TaskQueue::TaskQueue(std::string name, Kind kind) 
    : TaskQueue(CreateTaskQueue(std::move(name), kind)) {}

TaskQueue::TaskQueue(std::unique_ptr<TaskQueueImpl, TaskQueueImpl::Deleter> task_queue_impl) 
    : impl_(task_queue_impl.release()) {}

TaskQueue::~TaskQueue() {
    PLOG_VERBOSE << __FUNCTION__ << " will destroy.";
    // Do NOT invalidate `impl_` until Delete returns to 
    // make sure all remained tasks will be executed with a 
    // valid pointer.
    impl_->Delete();
    PLOG_VERBOSE << __FUNCTION__ << " did destroy.";
}

void TaskQueue::Sync(std::function<void()> handler) const {
    if (IsCurrent()) {
        handler();
    } else {
        // std::unique_lock<std::mutex> lock(mutex_);
        impl_->Post([this, handler=std::move(handler)]{
            handler();
            // cond_.notify_one();
            event_.Set();
        });
        // cond_.wait(lock);
        event_.WaitForever();
    }
}

void TaskQueue::Async(std::function<void()> handler) {
    impl_->Post(std::move(handler));
}

void TaskQueue::AsyncAfter(TimeDelta delay, std::function<void()> handler) {
    impl_->PostDelayed(delay, std::move(handler));
}

bool TaskQueue::IsCurrent() const {
    return impl_->IsCurrent();
}

} // namespace naivertc