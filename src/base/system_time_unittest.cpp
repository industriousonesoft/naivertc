#include "base/system_time.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(SystemTimeTest, SystemTimeInNanos) {
    int64_t time_in_ns = SystemTimeInNanos();

    EXPECT_GT(time_in_ns, 0);
}

} // test
} // namespace naivertc