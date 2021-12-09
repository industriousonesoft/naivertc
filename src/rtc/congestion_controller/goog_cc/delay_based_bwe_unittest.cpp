#include "rtc/congestion_controller/goog_cc/delay_based_bwe_unittest_helper.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(DelayBasedBweTest, Test) {
    int a = 7;
    int b = 20;
    int c = (b * 1.0) / a + 0.5;
    EXPECT_EQ(c, 3);
}
    
} // namespace test
} // namespace naivertc
