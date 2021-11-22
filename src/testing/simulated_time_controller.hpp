#ifndef _TESTING_SIMULATED_TIME_CONTROLLER_H_
#define _TESTING_SIMULATED_TIME_CONTROLLER_H_

#include "base/defines.hpp"
#include "rtc/base/synchronization/yield_policy.hpp"
#include "base/thread_annotation.hpp"
#include "testing/simulated_task_queue.hpp"
#include "common/thread_utils.hpp"
#include "testing/simulated_clock.hpp"

#include <mutex>
#include <vector>
#include <list>

namespace naivertc {

// SimulatedTimeController
class RTC_CPP_EXPORT SimulatedTimeController : public YieldInterface {
public:
    explicit SimulatedTimeController(Timestamp start_time);
    ~SimulatedTimeController() override;

    std::unique_ptr<SimulatedTaskQueue, SimulatedTaskQueue::Deleter> CreateTaskQueue();

    Clock* GetClock();
    Timestamp CurrentTime() const;
    Timestamp NextRunTime() const;

    void AdvanceTime(TimeDelta duration);

    void Register(SimulatedSequenceRunner* runner) RTC_LOCKS_EXCLUDED(lock_);
    void Deregister(SimulatedSequenceRunner* runner) RTC_LOCKS_EXCLUDED(lock_);

private:
    void AdvanceTimeTo(Timestamp target_time);
    void RunReadyRunners();

private:
    mutable std::mutex time_lock_;
    mutable std::mutex lock_;

    const PlatformThreadId thread_id_;
    Timestamp current_time_ RTC_GUARDED_BY(time_lock_);
    // Protected atomically.
    SimulatedClock sim_clock_;

    std::vector<SimulatedSequenceRunner *> runners_ RTC_GUARDED_BY(lock_);
    std::list<SimulatedSequenceRunner *> ready_runners_ RTC_GUARDED_BY(lock_);

};
    
} // namespace naivertc


#endif // _TESTING_SIMULATED_TIME_CONTROLLER_H_`