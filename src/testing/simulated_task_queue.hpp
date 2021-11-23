#ifndef _TESTING_SIMULATED_TASK_QUEUE_H_
#define _TESTING_SIMULATED_TASK_QUEUE_H_

#include "base/defines.hpp"
#include "base/thread_annotation.hpp"
#include "testing/simulated_sequence_runner.hpp"

#include <mutex>
#include <deque>
#include <map>

namespace naivertc {

class SimulatedTimeController;

// QueuedTask
class RTC_CPP_EXPORT QueuedTask {
public:
    virtual ~QueuedTask() = default;
    virtual bool Run() = 0;
};

// SimulatedTaskQueue
class RTC_CPP_EXPORT SimulatedTaskQueue : public SimulatedSequenceRunner {
public:
    struct Deleter {
        void operator()(SimulatedTaskQueue* task_queue) const { task_queue->Delete(); }
    };
public:
    SimulatedTaskQueue(SimulatedTimeController* handler);
    ~SimulatedTaskQueue() override;

    // Provides next run time.
    Timestamp GetNextRunTime() const RTC_LOCKS_EXCLUDED(lock_) override;
    // Runs all ready tasks next run time.
    void RunReady(Timestamp at_time) RTC_LOCKS_EXCLUDED(lock_) override;

    void Async(std::unique_ptr<QueuedTask> task) RTC_LOCKS_EXCLUDED(lock_);
    void AsyncAfter(TimeDelta due_time, std::unique_ptr<QueuedTask> task) RTC_LOCKS_EXCLUDED(lock_);

private:
    void Delete();
private:
    SimulatedTimeController* const handler_;

    mutable std::mutex lock_;

    using ReadyTaskDeque = std::deque<std::unique_ptr<QueuedTask>>;
    ReadyTaskDeque ready_tasks_ RTC_GUARDED_BY(lock_);
    using DelayedTaskMap = std::map<Timestamp, std::vector<std::unique_ptr<QueuedTask>>>;
    DelayedTaskMap delayed_tasks_ RTC_GUARDED_BY(lock_);

    Timestamp next_run_time_ RTC_GUARDED_BY(lock_) = Timestamp::PlusInfinity();
};

} // namespace naivertc

#endif