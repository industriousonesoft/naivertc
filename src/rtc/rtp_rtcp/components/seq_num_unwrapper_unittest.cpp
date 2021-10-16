#include "rtc/rtp_rtcp/components/seq_num_unwrapper.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(SeqNumUnwrapper, PreserveStartValue) {
    SeqNumUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(123, unwrapper.Unwrap(123));
}

TEST(SeqNumUnwrapper, ForwardWrap) {
    SeqNumUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(255, unwrapper.Unwrap(255));
    EXPECT_EQ(256, unwrapper.Unwrap(0));
}

TEST(SeqNumUnwrapper, ForwardWrapWithDivisor) {
    SeqNumUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(30, unwrapper.Unwrap(30));
    EXPECT_EQ(36, unwrapper.Unwrap(3));
}

TEST(SeqNumUnwrapper, BackWardWrap) {
    SeqNumUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(-2, unwrapper.Unwrap(254));
}

TEST(SeqNumUnwrapper, BackWardWrapWithDivisor) {
    SeqNumUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(-2, unwrapper.Unwrap(31));
}

TEST(SeqNumUnwrapper, Unwrap) {
    SeqNumUnwrapper<uint16_t> unwrapper;
    const uint16_t kMax = std::numeric_limits<uint16_t>::max();
    const uint16_t kMaxDist = kMax / 2 + 1;

    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(kMaxDist, unwrapper.Unwrap(kMaxDist));
    EXPECT_EQ(0, unwrapper.Unwrap(0));

    EXPECT_EQ(kMaxDist, unwrapper.Unwrap(kMaxDist));
    EXPECT_EQ(kMax, unwrapper.Unwrap(kMax));
    EXPECT_EQ(kMax + 1, unwrapper.Unwrap(0));
    EXPECT_EQ(kMax, unwrapper.Unwrap(kMax));
    EXPECT_EQ(kMaxDist, unwrapper.Unwrap(kMaxDist));
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    }

    TEST(SeqNumUnwrapper, UnwrapOddDivisor) {
    SeqNumUnwrapper<uint8_t, 11> unwrapper;

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

TEST(SeqNumUnwrapper, ManyForwardWraps) {
    const int kLargeNumber = 4711;
    const int kMaxStep = kLargeNumber / 2;
    const int kNumWraps = 100;
    SeqNumUnwrapper<uint16_t, kLargeNumber> unwrapper;

    uint16_t next_unwrap = 0;
    int64_t expected = 0;
    for (int i = 0; i < kNumWraps * 2 + 1; ++i) {
        EXPECT_EQ(expected, unwrapper.Unwrap(next_unwrap));
        expected += kMaxStep;
        next_unwrap = (next_unwrap + kMaxStep) % kLargeNumber;
    }
}

TEST(SeqNumUnwrapper, ManyBackwardWraps) {
    const int kLargeNumber = 4711;
    const int kMaxStep = kLargeNumber / 2;
    const int kNumWraps = 100;
    SeqNumUnwrapper<uint16_t, kLargeNumber> unwrapper;

    uint16_t next_unwrap = 0;
    int64_t expected = 0;
    for (uint16_t i = 0; i < kNumWraps * 2 + 1; ++i) {
        EXPECT_EQ(expected, unwrapper.Unwrap(next_unwrap));
        expected -= kMaxStep;
        next_unwrap = (next_unwrap + kMaxStep + 1) % kLargeNumber;
    }
}

TEST(SeqNumUnwrapper, UnwrapForward) {
    SeqNumUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(255, unwrapper.Unwrap(255));
    EXPECT_EQ(256, unwrapper.UnwrapForward(0));
    EXPECT_EQ(511, unwrapper.UnwrapForward(255));
}

TEST(SeqNumUnwrapper, UnwrapForwardWithDivisor) {
    SeqNumUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(30, unwrapper.UnwrapForward(30));
    EXPECT_EQ(36, unwrapper.UnwrapForward(3));
    EXPECT_EQ(63, unwrapper.UnwrapForward(30));
}

TEST(SeqNumUnwrapper, UnwrapBackwards) {
    SeqNumUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(0, unwrapper.UnwrapBackwards(0));
    EXPECT_EQ(-2, unwrapper.UnwrapBackwards(254));
    EXPECT_EQ(-256, unwrapper.UnwrapBackwards(0));
}

TEST(SeqNumUnwrapper, UnwrapBackwardsWithDivisor) {
    SeqNumUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(-2, unwrapper.UnwrapBackwards(31));
    EXPECT_EQ(-33, unwrapper.UnwrapBackwards(0));
}
    
} // namespace test
} // namespace naivertc
