#ifndef _RTC_BASE_TASK_UTILS_TASK_QUEUE_CHECKER_H_
#define _RTC_BASE_TASK_UTILS_TASK_QUEUE_CHECKER_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

namespace naivertc {

// SequenceChecker is a helper class used to help verify that
// some methods of a class are called on the same task queue.
class RTC_CPP_EXPORT SequenceChecker {
public:
    // The task queue checker will attacked to the queue 
    // calling this constructor method.
    SequenceChecker();
    ~SequenceChecker();

    // Return true if the checker is running on the queue 
    // in which the checker was created before.
    bool IsCurrent() const;

private:
    const TaskQueueImpl* const attacked_queue_;
};
    
} // namespace naivertc


#endif