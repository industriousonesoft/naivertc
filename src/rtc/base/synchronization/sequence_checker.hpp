#ifndef _RTC_BASE_SYNCHRONIZATION_SEQUENCE_CHECKER_H_
#define _RTC_BASE_SYNCHRONIZATION_SEQUENCE_CHECKER_H_

#include "base/defines.hpp"
#include "base/thread_annotation.hpp"

#include <mutex>

namespace naivertc {

class TaskQueueImpl;

// SequenceChecker is a helper class used to help verify that
// some methods of a class are called on the same task queue.
class SequenceChecker {
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
    void Detach();

private:
    mutable std::mutex lock_;
    mutable bool attached_ RTC_GUARDED_BY(lock_);
    mutable const TaskQueueImpl* attached_queue_ RTC_GUARDED_BY(lock_);
};

#define RTC_RUN_ON(x)   \
    assert((x)->IsCurrent() && "TaskQueue doesn't match.")
    
} // namespace naivertc


#endif