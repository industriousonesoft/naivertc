#ifndef _RTC_BASE_TASK_UTILS_PENDING_TASK_SAFETY_FLAG_H_
#define _RTC_BASE_TASK_UTILS_PENDING_TASK_SAFETY_FLAG_H_

#include "rtc/base/synchronization/sequence_checker.hpp"

namespace naivertc {

// The PendingTaskSafetyFlag and the ScopedTaskSafety are designed to address
// the issue where you have a task to be executed later that has references,
// but cannot guarantee that the referenced object is alive when the task is
// executed.
// This mechanism can be used with tasks that are created and destroyed
// on a single thread/task queue, and with tasks posted to the same
// thread/task queue, but tasks can be posted from any thread/task queue.
class PendingTaskSafetyFlag final {
public:
    static std::shared_ptr<PendingTaskSafetyFlag> Create();
    static std::shared_ptr<PendingTaskSafetyFlag> CreateDetached();
public:
    ~PendingTaskSafetyFlag() = default;

    bool alive() const;
    void SetAlive();
    void SetNotAlive();
    
protected:
    explicit PendingTaskSafetyFlag(bool alive) 
        : alive_(alive) {}

private:
    bool alive_ = true;
    SequenceChecker sequnce_checker_;
};

// This should be used by the class that wants tasks dropped after destruction.
// The requirement is that the instance has to be constructed and destructed on
// the same thread as the potentially dropped tasks would be running on.
class ScopedTaskSafety final {
public:
    ScopedTaskSafety() = default;
    ~ScopedTaskSafety() { flag_->SetNotAlive(); }

    // Returns a new reference to the safety flag.
    std::shared_ptr<PendingTaskSafetyFlag> flag() const { return flag_; }

private:
    std::shared_ptr<PendingTaskSafetyFlag> flag_ = PendingTaskSafetyFlag::Create();
};

} // namespace naivertc

#endif