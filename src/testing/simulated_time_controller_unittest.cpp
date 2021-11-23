#include "testing/simulated_time_controller.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {

constexpr Timestamp kStartTime = Timestamp::Seconds(1000);
    
} // namespace

MY_TEST(SimulatedTimeControllerTest, TaskIsStoppedOnStop) {
    const TimeDelta kShortInterval = TimeDelta::Millis(5);
    const TimeDelta kLongInterval = TimeDelta::Millis(20);
    const int kShortIntervalCount = 4;
    const int kMargin = 1;

    SimulatedTimeController time_simulation(kStartTime);
    auto task_queue = time_simulation.CreateTaskQueue();

}

} // namespace test
} // namespace naivertc