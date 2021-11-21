#include "rtc/base/numerics/histogram_percentile_counter.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(HistogramPercentileCounterTest, ReturnsCorrectPercentiles) {
    HistogramPercentileCounter counter(10);
    const std::vector<int> kTestValues = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                          11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    EXPECT_FALSE(counter.GetPercentile(0.5f));
    // Pairs of {fraction, percentile value} computed by hand
    // for `kTestValues`.
    const std::vector<std::pair<float, uint32_t>> kTestPercentiles = {
        {0.0f, 1},   {0.01f, 1},  {0.5f, 10}, {0.9f, 18},
        {0.95f, 19}, {0.99f, 20}, {1.0f, 20}};
    for (int value : kTestValues) {
        counter.Add(value);
    }
    for (const auto& test_percentile : kTestPercentiles) {
        EXPECT_EQ(test_percentile.second, counter.GetPercentile(test_percentile.first).value_or(0));
    }
}

MY_TEST(HistogramPercentileCounterTest, HandlesEmptySequence) {
    HistogramPercentileCounter counter(10);
    EXPECT_FALSE(counter.GetPercentile(0.5f).has_value());
    counter.Add(1u);
    EXPECT_TRUE(counter.GetPercentile(0.5f).has_value());
    EXPECT_EQ(1u, counter.GetPercentile(0.5f).value());
}

} // namespace test
} // namespace naivertc
