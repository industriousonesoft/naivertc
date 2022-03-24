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

void TaskQueue::Post(std::unique_ptr<QueuedTask> task) {
    impl_->Post(std::move(task));
}

void TaskQueue::PostDelayed(TimeDelta delay, std::unique_ptr<QueuedTask> task) {
    impl_->PostDelayed(delay, std::move(task));
}

bool TaskQueue::IsCurrent() const {
    return impl_->IsCurrent();
}

} // namespace naivertc