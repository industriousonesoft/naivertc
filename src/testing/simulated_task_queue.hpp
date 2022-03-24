#ifndef _TESTING_SIMULATED_TASK_QUEUE_H_
#define _TESTING_SIMULATED_TASK_QUEUE_H_

#include "base/defines.hpp"
#include "base/thread_annotation.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"
#include "testing/simulated_sequence_runner.hpp"

#include <mutex>
#include <deque>
#include <map>

namespace naivertc {

class SimulatedTimeController;

// SimulatedTaskQueue
class SimulatedTaskQueue : public TaskQueueImpl, 
                           public SimulatedSequenceRunner {
public:
    SimulatedTaskQueue(SimulatedTimeController* handler);
    ~SimulatedTaskQueue() override;

    // SimulatedSequenceRunner interface
    // Provides next run time.
    Timestamp GetNextRunTime() const RTC_LOCKS_EXCLUDED(lock_) override;
    // Runs all ready tasks next run time.
    void RunReady(Timestamp at_time) RTC_LOCKS_EXCLUDED(lock_) override;

    // TaskQueueImpl interface
    void Delete() RTC_LOCKS_EXCLUDED(lock_) override;
    void Post(std::unique_ptr<QueuedTask> task) RTC_LOCKS_EXCLUDED(lock_) override;
    void PostDelayed(TimeDelta delay, std::unique_ptr<QueuedTask> task) RTC_LOCKS_EXCLUDED(lock_) override;

private:
    SimulatedTimeController* const time_controller_;

    mutable std::mutex lock_;

    using ReadyTaskDeque = std::deque<std::unique_ptr<QueuedTask>>;
    ReadyTaskDeque ready_tasks_ RTC_GUARDED_BY(lock_);
    using DelayedTaskMap = std::map<Timestamp, std::vector<std::unique_ptr<QueuedTask>>>;
    DelayedTaskMap delayed_tasks_ RTC_GUARDED_BY(lock_);

    Timestamp next_run_time_ RTC_GUARDED_BY(lock_) = Timestamp::PlusInfinity();
};

} // namespace naivertc

#endif