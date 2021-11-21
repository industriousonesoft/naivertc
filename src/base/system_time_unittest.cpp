#include "base/system_time.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(SystemTimeTest, SystemTimeInNanos) {
    int64_t time_in_ns = SystemTimeInNanos();
    EXPECT_GT(time_in_ns, 0);
}

} // test
} // namespace naivertc