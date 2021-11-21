#ifndef _TESTING_SIMULATED_TIME_CONTROLLER_H_
#define _TESTING_SIMULATED_TIME_CONTROLLER_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/synchronization/yield_policy.hpp"
#include "rtc/base/thread_annotation.hpp"

#include <mutex>
#include <vector>
#include <list>

namespace naivertc {

// SimulatedSequenceRunner
class RTC_CPP_EXPORT SimulatedSequenceRunner {
public:
    virtual ~SimulatedSequenceRunner() = default;

    // Provides next run time.
    virtual Timestamp GetNextRunTime() const = 0;
    // Runs all ready tasks next run time.
    virtual void RunReady(Timestamp at_time) = 0;
};

// SimulatedTimeController
class RTC_CPP_EXPORT SimulatedTimeController : public YieldInterface {
public:
    explicit SimulatedTimeController(Timestamp start_time);
    ~SimulatedTimeController() override;

    Timestamp CurrentTime() const;
    void AdvanceTimeTo(Timestamp target_time);

    void Register(SimulatedSequenceRunner* runner) RTC_LOCKS_EXCLUDED(lock_);
    void Deregister(SimulatedSequenceRunner* runner) RTC_LOCKS_EXCLUDED(lock_);

private:
    mutable std::mutex time_lock_;
    mutable std::mutex lock_;

    Timestamp current_time_ RTC_GUARDED_BY(time_lock_);

    std::vector<SimulatedSequenceRunner *> runners_ RTC_GUARDED_BY(lock_);
    std::list<SimulatedSequenceRunner *> ready_runners_ RTC_GUARDED_BY(lock_);

};
    
} // namespace naivertc


#endif // _TESTING_SIMULATED_TIME_CONTROLLER_H_`