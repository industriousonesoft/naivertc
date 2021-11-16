#include "common/task_queue.hpp"
#include "common/event.hpp"
#include "common/utils_time.hpp"

#include <gtest/gtest.h>
#include <future>

#define ENABLE_UNIT_TESTS 1
#include "../testing/unittest_defines.hpp"

using namespace testing;

namespace naivertc {
namespace test {

void CheckCurrent(Event* signal, TaskQueue* queue) {
    EXPECT_TRUE(queue->is_in_current_queue());
    if (signal) {
        signal->Set();
    }
}

MY_TEST(TaskQueueTest, SyncPost) {
    TaskQueue task_queue;
    int ret = 1;
    ret = task_queue.Sync<int>([]() {
        return 100;
    });
    EXPECT_EQ(ret, 100);
}

MY_TEST(TaskQueueTest, AsyncPost) {
    TaskQueue task_queue;
    Event event;
    task_queue.Async([&task_queue, &event](){
        CheckCurrent(&event, &task_queue);
    });
    EXPECT_TRUE(event.Wait(1000));
}

MY_TEST(TaskQueueTest, AsyncDelayedPost) {
    TaskQueue task_queue;
    Event event;
    int64_t start = utils::time::TimeInSec();
    task_queue.AsyncAfter(3 /* seconds */, [&task_queue, &event](){
        CheckCurrent(&event, &task_queue);
    });
    EXPECT_TRUE(event.Wait(Event::kForever));
    int64_t end = utils::time::TimeInSec();
    EXPECT_GE(end-start, 3);
    EXPECT_NEAR(end-start, 3, 1);
}

MY_TEST(TaskQueueTest, AsyncPostBehindDelayedPost) {
    TaskQueue task_queue;
    Event event1;
    int val = 1;
    task_queue.AsyncAfter(3 /* seconds */, [&task_queue, &event1, &val](){
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
    EXPECT_TRUE(event1.Wait(Event::kForever));
    EXPECT_TRUE(event2.Wait(Event::kForever));
}

} // namespace test
} // namespace naivertc

