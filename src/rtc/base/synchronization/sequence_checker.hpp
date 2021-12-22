#ifndef _RTC_BASE_SYNCHRONIZATION_SEQUENCE_CHECKER_H_
#define _RTC_BASE_SYNCHRONIZATION_SEQUENCE_CHECKER_H_

#include "base/defines.hpp"
#include "base/thread_annotation.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

// #include <mutex>

namespace naivertc {

// SequenceChecker is a helper class used to help verify that
// some methods of a class are called on the same task queue.
class RTC_CPP_EXPORT SequenceChecker {
public:
    // The task queue checker will be attached to the queue 
    // calling this constructor method.
    SequenceChecker();
    ~SequenceChecker();

    // Return true if the checker is running on the queue 
    // in which the checker was created before.
    bool IsCurrent() const;
    // Changes the task queue or thread that is checked for in IsCurrent. This can
    // be useful when an object may be created on one task queue / thread and then
    // used exclusively on another thread.
    // void Detach();

private:
    // mutable std::mutex lock_;
    // mutable bool attached_ RTC_GUARDED_BY(lock_);
    const TaskQueueImpl* attached_queue_ ;//RTC_GUARDED_BY(lock_);
};
    
} // namespace naivertc


#endif