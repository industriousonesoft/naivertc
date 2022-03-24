#ifndef _RTC_BASE_TASK_UTILS_QUEUED_TASK_H_
#define _RTC_BASE_TASK_UTILS_QUEUED_TASK_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/pending_task_safety_flag.hpp"

#include <type_traits>

namespace naivertc {

// QueuedTask
class QueuedTask {
public:
    virtual ~QueuedTask() = default;
    virtual void Run() = 0;
};

template <typename Closure>
class ClosureTask : public QueuedTask {
public:
    explicit ClosureTask(Closure&& closure)
       : closure_(std::forward<Closure>(closure)) {}

private:
    void Run() override {
        closure_();
    }

private:
    typename std::decay<Closure>::type closure_;
};

// SafetyClosureTask
template <typename Closure>
class SafetyClosureTask : public QueuedTask {
public:
    explicit SafetyClosureTask(Closure&& closure,
                               std::shared_ptr<PendingTaskSafetyFlag> safety_flag) 
        : closure_(std::forward<Closure>(closure)),
          safety_flag_(std::move(safety_flag)) {};
  
private: 
    void Run() override {
        if (safety_flag_->alive()) {
            closure_();
        }
    }

private:
    typename std::decay<Closure>::type closure_;
    std::shared_ptr<PendingTaskSafetyFlag> safety_flag_;
};

// Convenience function to construct closures that can be passed directly

template<typename Closure>
std::unique_ptr<QueuedTask> ToQueuedTask(Closure&& closure) {
    return std::make_unique<ClosureTask<Closure>>(std::forward<Closure>(closure));
}

template<typename Closure>
std::unique_ptr<QueuedTask> ToQueuedTask(const ScopedTaskSafety& safety,
                                         Closure&& closure) {
    return std::make_unique<SafetyClosureTask<Closure>>(std::forward<Closure>(closure), safety.flag());
}

template<typename Closure>
std::unique_ptr<QueuedTask> ToQueuedTask(std::shared_ptr<PendingTaskSafetyFlag> safety_flag,
                                         Closure&& closure) {
    return std::make_unique<SafetyClosureTask<Closure>>(std::forward<Closure>(closure), safety_flag);
}

    
} // namespace naivertc


#endif