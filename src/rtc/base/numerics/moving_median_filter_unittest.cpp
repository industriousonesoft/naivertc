#include "rtc/base/numerics/moving_median_filter.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(MovingMedianFilterTest, NoSamples) {
    MovingMedianFilter<int> filter(2);
    EXPECT_EQ(0, filter.GetFilteredValue());
    EXPECT_EQ(0u, filter.stored_sample_count());
}

MY_TEST(MovingMedianFilterTest, ReturnsMovingMedianWindow5) {
    MovingMedianFilter<int> filter(5);
    const int64_t kSamples[5] = {1, 5, 2, 3, 4};
    const int64_t kExpectedFilteredValues[5] = {1, 1, 2, 2, 3};
    for (size_t i = 0; i < 5; ++i) {
        filter.Insert(kSamples[i]);
        EXPECT_EQ(kExpectedFilteredValues[i], filter.GetFilteredValue());
        EXPECT_EQ(i + 1, filter.stored_sample_count());
    }
}

MY_TEST(MovingMedianFilterTest, ReturnsMovingMedianWindow3) {
    MovingMedianFilter<int> filter(3);
    const int64_t kSamples[5] = {1, 5, 2, 3, 4};
    const int64_t kExpectedFilteredValues[5] = {1, 1, 2, 3, 3};
    for (int i = 0; i < 5; ++i) {
        filter.Insert(kSamples[i]);
        EXPECT_EQ(kExpectedFilteredValues[i], filter.GetFilteredValue());
        EXPECT_EQ(std::min<size_t>(i + 1, 3), filter.stored_sample_count());
    }
}

MY_TEST(MovingMedianFilterTest, ReturnsMovingMedianWindow1) {
    MovingMedianFilter<int> filter(1);
    const int64_t kSamples[5] = {1, 5, 2, 3, 4};
    const int64_t kExpectedFilteredValues[5] = {1, 5, 2, 3, 4};
    for (int i = 0; i < 5; ++i) {
        filter.Insert(kSamples[i]);
        EXPECT_EQ(kExpectedFilteredValues[i], filter.GetFilteredValue());
        EXPECT_EQ(1u, filter.stored_sample_count());
    }
}
    
} // namespace test
} // namespace naivertc
