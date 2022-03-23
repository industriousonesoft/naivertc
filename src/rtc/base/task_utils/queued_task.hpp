#ifndef _RTC_BASE_TASK_UTILS_QUEUED_TASK_H_
#define _RTC_BASE_TASK_UTILS_QUEUED_TASK_H_

#include "base/defines.hpp"

#include <type_traits>

namespace naivertc {

// QueuedTask
class QueuedTask {
public:
    explicit QueuedTask(std::function<void()>&& closure)
        : closure_(std::move(closure)) {}

    void Run() {
        closure_();
    }

    void operator()() {
        Run();
    }

private:
    std::function<void()> closure_;
};

    
} // namespace naivertc


#endif