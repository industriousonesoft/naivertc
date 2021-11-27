#include "rtc/base/synchronization/sequence_checker.hpp"

namespace naivertc {

SequenceChecker::SequenceChecker() 
    : attacked_queue_(TaskQueueImpl::Current()) {}

SequenceChecker::~SequenceChecker() = default;

bool SequenceChecker::IsCurrent() const {
    return attacked_queue_ == TaskQueueImpl::Current();
}
    
} // namespace naivertc
