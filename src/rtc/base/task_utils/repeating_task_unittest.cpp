#include "rtc/base/task_utils/repeating_task.hpp"
#include "rtc/base/time/clock_real_time.hpp"
#include "rtc/base/synchronization/event.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(RepeatingTaskTest, StopInternally) {
    std::shared_ptr<Clock> clock = std::make_shared<RealTimeClock>();
    std::shared_ptr<TaskQueue> task_queue = std::make_shared<TaskQueue>();
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


MY_TEST(RepeatingTaskTest, StopExternally) {
    std::shared_ptr<Clock> clock = std::make_shared<RealTimeClock>();
    std::shared_ptr<TaskQueue> task_queue = std::make_shared<TaskQueue>();
    Event event;
    int counter = 0;
    std::unique_ptr<RepeatingTask> repeating_task = RepeatingTask::DelayedStart(clock, task_queue, TimeDelta::Seconds(1), [&counter](){
        ++counter;
        return TimeDelta::Seconds(1);
    });
    EXPECT_EQ(counter, 0);
    task_queue->AsyncAfter(3 /* 3s */, [&](){
        repeating_task->Stop();
        event.Set();
    });
    event.WaitForever();
    EXPECT_EQ(counter, 2);
}
    
} // namespace test
} // namespace naivertc
