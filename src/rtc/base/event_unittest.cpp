#include "rtc/base/event.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(EventTest, InitiallySignaled) {
    Event event(false, true);
    ASSERT_TRUE(event.Wait(0));
}

MY_TEST(EventTest, ManualReset) {
    Event event(true, false);
    ASSERT_FALSE(event.Wait(0));

    event.Set();
    ASSERT_TRUE(event.Wait(0));
    ASSERT_TRUE(event.Wait(0));

    event.Reset();
    ASSERT_FALSE(event.Wait(0));
}

MY_TEST(EventTest, AutoReset) {
    Event event;
    ASSERT_FALSE(event.Wait(0));

    event.Set();
    ASSERT_TRUE(event.Wait(0));
    ASSERT_FALSE(event.Wait(0));
}
    
} // namespace test
} // namespace naivertc
