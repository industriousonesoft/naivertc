#include "rtc/base/synchronization/yield_policy.hpp"
#include "rtc/base/synchronization/event.hpp"

#include <thread>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

// MockYieldHandler
class MockYieldHandler : public YieldInterface {
public:
    MOCK_METHOD(void, YieldExecution, (), (override));
};

MY_TEST(YieldPolicyTest, HandlerReceivesYieldSignalWhenSet) {
    ::testing::StrictMock<MockYieldHandler> handler;
    {
        Event event;
        EXPECT_CALL(handler, YieldExecution()).Times(1);
        ScopedYieldPolicy policy(&handler);
        event.Set();
        // Event will trigger `YieldExecution` when it's ready to wait.
        // NOTE: `WaitForever` will never block the thread as it was set before.
        event.WaitForever();
    }
    {
        Event event;
        EXPECT_CALL(handler, YieldExecution()).Times(0);
        event.Set();
        event.WaitForever();
    }
}

MY_TEST(YieldPolicyTest, IsThreadLocal) {
    Event events[3];
    // The other thread will get started automatically after created.
    std::thread other_thread([&](){
        ::testing::StrictMock<MockYieldHandler> other_local_handler;
        // The local handler is never called as we never Wait on this thread.
        EXPECT_CALL(other_local_handler, YieldExecution()).Times(0);
        ScopedYieldPolicy policy(&other_local_handler);
        events[0].Set();
        events[1].Set();
        events[2].Set();
    });

    // Waiting until the other thread has entered the scoped policy.
    events[0].WaitForever();
    // Wait on this thread should not trigger the handler of `other_local_handler` as it
    // belong to the other thread.
    events[1].WaitForever();

    // We can set a policy that's active on this thread independently.
    ::testing::StrictMock<MockYieldHandler> main_handler;
    EXPECT_CALL(main_handler, YieldExecution()).Times(1);
    ScopedYieldPolicy policy(&main_handler);
    events[2].WaitForever();
    // Block the current thread until the other thread was done.
    other_thread.join();
}

} // namespace test
} // namespace naivertc