#ifndef _TESTING_SIMULATED_SEQUENCE_RUNNER_H_
#define _TESTING_SIMULATED_SEQUENCE_RUNNER_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"

namespace naivertc {

// SimulatedSequenceRunner
class SimulatedSequenceRunner {
public:
    virtual ~SimulatedSequenceRunner() = default;

    // Provides next run time.
    virtual Timestamp GetNextRunTime() const = 0;
    // Runs all ready tasks next run time.
    virtual void RunReady(Timestamp at_time) = 0;
};

} // namespace naivertc

#endif