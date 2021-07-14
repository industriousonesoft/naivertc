#include "common/task_queue.hpp"

#include <gtest/gtest.h>
#include <future>

using namespace testing;

namespace naivertc {
namespace test {

class TaskQueueTest : public Test {

protected:
    void SetUp() override {

    }

    void TearDown() override {

    }

    TaskQueue task_queue_;
    std::promise<bool> promise_;
};

TEST_F(TaskQueueTest, CheckIsInCurrentQueue) {
    EXPECT_FALSE(task_queue_.is_in_current_queue());
    task_queue_.Post([this](){
        EXPECT_TRUE(task_queue_.is_in_current_queue());
    });
}

TEST_F(TaskQueueTest, SyncPost) {
    int ret = 1;
    ret = task_queue_.SyncPost<int>([]() {
        return 100;
    });
    EXPECT_EQ(ret, 100);
}

// // TODO: promise not work??
// TEST_F(TaskQueueTest, PostDelay) {
//     auto start = boost::posix_time::second_clock::universal_time();
//     std::promise<bool> promise;
//     auto future = promise_.get_future();
//     promise_ = std::move(promise);
//     task_queue_.PostDelay(3, [this, start](){
//         EXPECT_TRUE(task_queue_.is_in_current_queue());
//         auto end = boost::posix_time::second_clock::universal_time();
//         auto delay_in_sec = end - start;
//         EXPECT_GE(delay_in_sec.seconds(), 3);
//         promise_.set_value(true);
//     });
//     future.get();
// }

} // namespace test
} // namespace naivertc

