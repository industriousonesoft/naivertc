#include "rtc/base/synchronization/sequence_checker.hpp"

namespace naivertc {

SequenceChecker::SequenceChecker() 
    : attached_queue_(TaskQueueImpl::Current()) {
    assert(attached_queue_ != nullptr && "No task queue can be attached to.");
}

SequenceChecker::~SequenceChecker() = default;

bool SequenceChecker::IsCurrent() const {
    return attached_queue_ == TaskQueueImpl::Current();
}

TaskQueueImpl* SequenceChecker::attached_queue() const {
    return attached_queue_;
}
    
} // namespace naivertc
