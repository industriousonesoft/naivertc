#include "rtc/base/task_utils/task_queue_checker.hpp"

namespace naivertc {

TaskQueueChecker::TaskQueueChecker() 
    : attacked_queue_(TaskQueueImpl::Current()) {}

TaskQueueChecker::~TaskQueueChecker() = default;

bool TaskQueueChecker::IsCurrent() const {
    return attacked_queue_ == TaskQueueImpl::Current();
}
    
} // namespace naivertc
