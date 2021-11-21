#include "rtc/rtp_rtcp/components/num_unwrapper.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(NumUnwrapperTest, PreserveStartValue) {
    NumberUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(123, unwrapper.Unwrap(123));
}

MY_TEST(NumUnwrapperTest, ForwardWrap) {
    NumberUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(255, unwrapper.Unwrap(255));
    EXPECT_EQ(256, unwrapper.Unwrap(0));
}

MY_TEST(NumUnwrapperTest, ForwardWrapWithDivisor) {
    NumberUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(30, unwrapper.Unwrap(30));
    EXPECT_EQ(36, unwrapper.Unwrap(3));
}

MY_TEST(NumUnwrapperTest, BackWardWrap) {
    NumberUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(-2, unwrapper.Unwrap(254, false));
}

MY_TEST(NumUnwrapperTest, BackWardWrapWithDivisor) {
    NumberUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(-2, unwrapper.Unwrap(31, false));
}

MY_TEST(NumUnwrapperTest, Unwrap) {
    NumberUnwrapper<uint16_t> unwrapper;
    const uint16_t kMax = std::numeric_limits<uint16_t>::max();
    const uint16_t kMaxDist = kMax / 2 + 1;

    EXPECT_EQ(0, unwrapper.Unwrap(0));
    // Forward
    EXPECT_EQ(kMaxDist, unwrapper.Unwrap(kMaxDist));
    // Backward
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    // Forward
    EXPECT_EQ(kMaxDist, unwrapper.Unwrap(kMaxDist));
    // Forward
    EXPECT_EQ(kMax, unwrapper.Unwrap(kMax));
    // Forward
    EXPECT_EQ(kMax + 1, unwrapper.Unwrap(0));
    // Backward
    EXPECT_EQ(kMax, unwrapper.Unwrap(kMax));
    // Backward
    EXPECT_EQ(kMaxDist, unwrapper.Unwrap(kMaxDist));
    // Backward
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    // Don't wrap backwards past 0.
    EXPECT_EQ(kMax, unwrapper.Unwrap(kMax));
}

MY_TEST(NumUnwrapperTest, UnwrapOddDivisor) {
    NumberUnwrapper<uint8_t, 11> unwrapper;

    EXPECT_EQ(10, unwrapper.Unwrap(10));
    EXPECT_EQ(11, unwrapper.Unwrap(0));
    EXPECT_EQ(16, unwrapper.Unwrap(5));
    EXPECT_EQ(21, unwrapper.Unwrap(10));
    EXPECT_EQ(22, unwrapper.Unwrap(0));
    EXPECT_EQ(17, unwrapper.Unwrap(6));
    EXPECT_EQ(12, unwrapper.Unwrap(1));
    EXPECT_EQ(7, unwrapper.Unwrap(7));
    EXPECT_EQ(2, unwrapper.Unwrap(2));
    EXPECT_EQ(0, unwrapper.Unwrap(0));
}

MY_TEST(NumUnwrapperTest, ManyForwardWraps) {
    const int kLargeNumber = 4711;
    const int kMaxStep = kLargeNumber / 2;
    const int kNumWraps = 100;
    NumberUnwrapper<uint16_t, kLargeNumber> unwrapper;

    uint16_t next_unwrap = 0;
    int64_t expected = 0;
    for (int i = 0; i < kNumWraps * 2 + 1; ++i) {
        EXPECT_EQ(expected, unwrapper.Unwrap(next_unwrap));
        expected += kMaxStep;
        next_unwrap = (next_unwrap + kMaxStep) % kLargeNumber;
    }
}

MY_TEST(NumUnwrapperTest, ManyBackwardWraps) {
    const int kLargeNumber = 4711;
    const int kMaxStep = kLargeNumber / 2;
    const int kNumWraps = 100;
    NumberUnwrapper<uint16_t, kLargeNumber> unwrapper;

    uint16_t next_unwrap = 0;
    int64_t expected = 0;
    for (uint16_t i = 0; i < kNumWraps * 2 + 1; ++i) {
        EXPECT_EQ(expected, unwrapper.Unwrap(next_unwrap, false /* disallow_negative */));
        expected -= kMaxStep;
        next_unwrap = (next_unwrap + kMaxStep + 1) % kLargeNumber;
    }
}

MY_TEST(NumUnwrapperTest, UnwrapForward) {
    NumberUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(255, unwrapper.Unwrap(255));
    EXPECT_EQ(256, unwrapper.UnwrapForward(0));
    EXPECT_EQ(511, unwrapper.UnwrapForward(255));
}

MY_TEST(NumUnwrapperTest, UnwrapForwardWithDivisor) {
    NumberUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(30, unwrapper.UnwrapForward(30));
    EXPECT_EQ(36, unwrapper.UnwrapForward(3));
    EXPECT_EQ(63, unwrapper.UnwrapForward(30));
}

MY_TEST(NumUnwrapperTest, UnwrapBackwards) {
    NumberUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(0, unwrapper.UnwrapBackwards(0));
    EXPECT_EQ(-2, unwrapper.UnwrapBackwards(254));
    EXPECT_EQ(-256, unwrapper.UnwrapBackwards(0));
}

MY_TEST(NumUnwrapperTest, UnwrapBackwardsWithDivisor) {
    NumberUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(-2, unwrapper.UnwrapBackwards(31));
    EXPECT_EQ(-33, unwrapper.UnwrapBackwards(0));
}
    
} // namespace test
} // namespace naivertc
