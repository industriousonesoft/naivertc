#include "testing/simulated_time_controller.hpp"
#include "rtc/base/task_utils/repeating_task.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
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
    std::atomic_int counter(0);
    auto repeating_task = RepeatingTask::Start(time_simulation.Clock(), task_queue, [&](){
        if (++counter >= kShortIntervalCount) {
            return kLongInterval;
        }
        return kShortInterval;
    });

    // Sleep long enough to go through the initial phase.
    time_simulation.AdvanceTime(kShortInterval * (kShortIntervalCount + kMargin));
    EXPECT_EQ(counter.load(), kShortIntervalCount);

    repeating_task->Stop();

    // Sleep long enough that the task would run at least once more if not
    // stopped.
    time_simulation.AdvanceTime(kLongInterval * 2);
    EXPECT_EQ(counter.load(), kShortIntervalCount);
}

MY_TEST(SimulatedTimeControllerTest, TaskCanStopItself) {
    SimulatedTimeController time_simulation(kStartTime);
    auto task_queue = time_simulation.CreateTaskQueue();
    std::atomic_int counter(0);
    std::unique_ptr<RepeatingTask> repeating_task = RepeatingTask::Start(time_simulation.Clock(), task_queue, [&](){
        ++counter;
        repeating_task->Stop();
        return TimeDelta::Millis(2);
    });
    time_simulation.AdvanceTime(TimeDelta::Millis(10));
    EXPECT_EQ(counter.load(), 1);
}

MY_TEST(SimulatedTimeControllerTest, DelayedTaskRunOnTime) {
    SimulatedTimeController time_simulation(kStartTime);
    auto task_queue = time_simulation.CreateTaskQueue();

    bool delay_task_executed = false;
    task_queue->AsyncAfter(TimeDelta::Millis(10), [&](){
        delay_task_executed = true;
    });

    time_simulation.AdvanceTime(TimeDelta::Millis(0));
    EXPECT_FALSE(delay_task_executed);

    time_simulation.AdvanceTime(TimeDelta::Millis(10));
    EXPECT_TRUE(delay_task_executed);
}

} // namespace test
} // namespace naivertc