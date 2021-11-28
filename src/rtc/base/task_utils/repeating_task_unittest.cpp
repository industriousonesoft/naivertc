#include "rtc/base/task_utils/repeating_task.hpp"
#include "rtc/base/time/clock_real_time.hpp"
#include "rtc/base/synchronization/event.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(RepeatingTaskTest, TaskStopedByReturningNonPositiveNumber) {
    std::shared_ptr<Clock> clock = std::make_shared<RealTimeClock>();
    std::shared_ptr<TaskQueue> task_queue = std::make_shared<TaskQueue>("RepeatingTaskTest.task_queue");
    Event event;
    int counter = 0;
    std::unique_ptr<RepeatingTask> repeating_task = RepeatingTask::DelayedStart(clock, task_queue, TimeDelta::Seconds(1), [&](){
        if (counter == 5) {
            event.Set();
            return TimeDelta::Seconds(0);
        } else {
            ++counter;
            return TimeDelta::Seconds(1);
        }
    });
    EXPECT_EQ(counter, 0);
    event.WaitForever();
    EXPECT_EQ(counter, 5);
}

MY_TEST(RepeatingTaskTest, TaskCanStopItself) {
    std::shared_ptr<Clock> clock = std::make_shared<RealTimeClock>();
    std::shared_ptr<TaskQueue> task_queue = std::make_shared<TaskQueue>("RepeatingTaskTest.task_queue");
    Event event;
    int counter = 0;
    std::unique_ptr<RepeatingTask> repeating_task = RepeatingTask::DelayedStart(clock, task_queue, TimeDelta::Seconds(1), [&](){
        if (counter == 5) {
            repeating_task->Stop();
            if (!repeating_task->Running()) {
                event.Set();
            } else {
                ++counter;
            }
        } else if (counter == 10) {
            event.Set();
            return TimeDelta::Seconds(0);
        } else {
            ++counter;
        }
        return TimeDelta::Seconds(1);
    });
    EXPECT_EQ(counter, 0);
    event.WaitForever();
    EXPECT_EQ(counter, 5);
}

MY_TEST(RepeatingTaskTest, StopExternally) {
    std::shared_ptr<Clock> clock = std::make_shared<RealTimeClock>();
    std::shared_ptr<TaskQueue> task_queue = std::make_shared<TaskQueue>("RepeatingTaskTest.task_queue");
    Event event;
    int counter = 0;
    std::unique_ptr<RepeatingTask> repeating_task = RepeatingTask::DelayedStart(clock, task_queue, TimeDelta::Seconds(1), [&counter](){
        ++counter;
        return TimeDelta::Seconds(1);
    });
    EXPECT_EQ(counter, 0);
    task_queue->AsyncAfter(TimeDelta::Seconds(3), [&](){
        repeating_task->Stop();
        ASSERT_FALSE(repeating_task->Running());
        event.Set();
    });
    event.WaitForever();
    EXPECT_EQ(counter, 2);
}
    
} // namespace test
} // namespace naivertc
