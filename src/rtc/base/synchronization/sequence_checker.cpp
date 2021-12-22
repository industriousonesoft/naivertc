#include "rtc/base/synchronization/sequence_checker.hpp"

namespace naivertc {

SequenceChecker::SequenceChecker() 
    : attached_(true),
      attached_queue_(TaskQueueImpl::Current()) {
    assert(attached_queue_ != nullptr && "No task queue can be attached to.");
}

SequenceChecker::~SequenceChecker() = default;

bool SequenceChecker::IsCurrent() const {
    const TaskQueueImpl* const curr_queue = TaskQueueImpl::Current();
    std::scoped_lock lock(lock_);
    if (!attached_) {
        attached_ = true;
        attached_queue_ = curr_queue;
        return true;
    }
    return attached_queue_ == curr_queue;
}

void SequenceChecker::Detach() {
    std::scoped_lock lock(lock_);
    attached_ = false;
}
    
} // namespace naivertc
