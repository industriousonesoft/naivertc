#include "common/task_queue.hpp"

#include <gtest/gtest.h>
#include <future>

using namespace testing;

namespace naivertc {
namespace test {

class Common_TaskQueueTest : public Test {

protected:
    void SetUp() override {

    }

    void TearDown() override {

    }

    TaskQueue task_queue_;
    std::promise<bool> promise_;
};

TEST_F(Common_TaskQueueTest, CheckIsInCurrentQueue) {
    EXPECT_FALSE(task_queue_->is_in_current_queue());
    task_queue_->Async([this](){
        EXPECT_TRUE(task_queue_->is_in_current_queue());
    });
}

TEST_F(Common_TaskQueueTest, AsyncPost) {
    int ret = 1;
    ret = task_queue_->Sync<int>([]() {
        return 100;
    });
    EXPECT_EQ(ret, 100);
}

} // namespace test
} // namespace naivertc

