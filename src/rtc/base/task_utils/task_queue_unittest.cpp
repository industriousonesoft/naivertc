#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/synchronization/event.hpp"
#include "common/utils_time.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

using namespace testing;

namespace naivertc {
namespace test {

void CheckCurrent(Event* signal, TaskQueue* queue) {
    EXPECT_TRUE(queue->IsCurrent());
    if (signal) {
        signal->Set();
    }
}

MY_TEST(TaskQueueTest, SyncPost) {
    TaskQueue task_queue("TaskQueueTest.SyncPost");
    int ret = 1;
    ret = task_queue.Sync<int>([]() {
        return 100;
    });
    EXPECT_EQ(ret, 100);
}

MY_TEST(TaskQueueTest, AsyncPost) {
    TaskQueue task_queue("TaskQueueTest.AsyncPost");
    Event event;
    task_queue.Async([&task_queue, &event](){
        CheckCurrent(&event, &task_queue);
    });
    EXPECT_TRUE(event.WaitForever());
}

MY_TEST(TaskQueueTest, MultipAsyncPost) {
    TaskQueue task_queue("TaskQueueTest.MultipAsyncPost");
    Event event;
    int val = 1;
    task_queue.Async([&](){
        val += 1;
    });
    task_queue.Async([&](){
        EXPECT_EQ(val, 2);
        CheckCurrent(&event, &task_queue);
    });
    EXPECT_TRUE(event.WaitForever());
}

MY_TEST(TaskQueueTest, AsyncDelayedPost) {
    TaskQueue task_queue("TaskQueueTest.AsyncDelayedPost");
    Event event;
    int64_t start = utils::time::TimeInSec();
    task_queue.AsyncAfter(TimeDelta::Seconds(3), [&task_queue, &event](){
        CheckCurrent(&event, &task_queue);
    });
    EXPECT_TRUE(event.WaitForever());
    int64_t end = utils::time::TimeInSec();
    EXPECT_GE(end-start, 3);
    EXPECT_NEAR(end-start, 3, 1);
}

MY_TEST(TaskQueueTest, MultipAsyncDelayedPost) {
    TaskQueue task_queue("TaskQueueTest.MultipAsyncDelayedPost");
    Event event1;
    int val = 1;
    task_queue.AsyncAfter(TimeDelta::Seconds(3), [&task_queue, &event1, &val](){
        val += 1;
        EXPECT_EQ(val, 2);
        CheckCurrent(&event1, &task_queue);
    });
    Event event2;
    task_queue.AsyncAfter(TimeDelta::Seconds(4), [&task_queue, &event2, &val](){
        val -= 1;
        EXPECT_EQ(val, 1);
        CheckCurrent(&event2, &task_queue);
    });
    EXPECT_TRUE(event1.WaitForever());
    EXPECT_TRUE(event2.WaitForever());
}

MY_TEST(TaskQueueTest, AsyncPostBehindDelayedPost) {
    TaskQueue task_queue("TaskQueueTest.AsyncPostBehindDelayedPost");
    Event event1;
    int val = 1;
    task_queue.AsyncAfter(TimeDelta::Seconds(3), [&task_queue, &event1, &val](){
        val += 1;
        EXPECT_EQ(val, 1);
        CheckCurrent(&event1, &task_queue);
    });
    Event event2;
    task_queue.Async([&task_queue, &event2, &val](){
        val -= 1;
        EXPECT_EQ(val, 0);
        CheckCurrent(&event2, &task_queue);
    });
    EXPECT_TRUE(event1.WaitForever());
    EXPECT_TRUE(event2.WaitForever());
}

} // namespace test
} // namespace naivertc

