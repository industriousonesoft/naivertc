#include "rtc/rtp_rtcp/components/seq_num_unwrapper.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(SeqNumUnwrapperTest, PreserveStartValue) {
    SeqNumUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(123, unwrapper.Unwrap(123));
}

TEST(SeqNumUnwrapperTest, ForwardWrap) {
    SeqNumUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(255, unwrapper.Unwrap(255));
    EXPECT_EQ(256, unwrapper.Unwrap(0));
}

TEST(SeqNumUnwrapperTest, ForwardWrapWithDivisor) {
    SeqNumUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(30, unwrapper.Unwrap(30));
    EXPECT_EQ(36, unwrapper.Unwrap(3));
}

TEST(SeqNumUnwrapperTest, BackWardWrap) {
    SeqNumUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(-2, unwrapper.Unwrap(254, false));
}

TEST(SeqNumUnwrapperTest, BackWardWrapWithDivisor) {
    SeqNumUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(-2, unwrapper.Unwrap(31, false));
}

TEST(SeqNumUnwrapperTest, Unwrap) {
    SeqNumUnwrapper<uint16_t> unwrapper;
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

TEST(SeqNumUnwrapperTest, UnwrapOddDivisor) {
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

TEST(SeqNumUnwrapperTest, ManyForwardWraps) {
    const int kLargeNumber = 4711;
    const int kMaxStep = kLargeNumber / 2;
    const int kNumWraps = 50;
    SeqNumUnwrapper<uint16_t, kLargeNumber> unwrapper;

    uint16_t next_unwrap = 0;
    int64_t expected = 0;
    for (int i = 0; i < kNumWraps * 2 + 1; ++i) {
        EXPECT_EQ(expected, unwrapper.Unwrap(next_unwrap));
        expected += kMaxStep;
        next_unwrap = (next_unwrap + kMaxStep) % kLargeNumber;
    }
}

TEST(SeqNumUnwrapperTest, ManyBackwardWraps) {
    const int kLargeNumber = 4711;
    const int kMaxStep = kLargeNumber / 2;
    const int kNumWraps = 100;
    SeqNumUnwrapper<uint16_t, kLargeNumber> unwrapper;

    uint16_t next_unwrap = 0;
    int64_t expected = 0;
    for (uint16_t i = 0; i < kNumWraps * 2 + 1; ++i) {
        EXPECT_EQ(expected, unwrapper.Unwrap(next_unwrap, false /* disallow_negative */));
        expected -= kMaxStep;
        next_unwrap = (next_unwrap + kMaxStep + 1) % kLargeNumber;
    }
}

TEST(SeqNumUnwrapperTest, UnwrapForward) {
    SeqNumUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(255, unwrapper.Unwrap(255));
    EXPECT_EQ(256, unwrapper.UnwrapForward(0));
    EXPECT_EQ(511, unwrapper.UnwrapForward(255));
}

TEST(SeqNumUnwrapperTest, UnwrapForwardWithDivisor) {
    SeqNumUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(30, unwrapper.UnwrapForward(30));
    EXPECT_EQ(36, unwrapper.UnwrapForward(3));
    EXPECT_EQ(63, unwrapper.UnwrapForward(30));
}

TEST(SeqNumUnwrapperTest, UnwrapBackwards) {
    SeqNumUnwrapper<uint8_t> unwrapper;
    EXPECT_EQ(0, unwrapper.UnwrapBackwards(0));
    EXPECT_EQ(-2, unwrapper.UnwrapBackwards(254));
    EXPECT_EQ(-256, unwrapper.UnwrapBackwards(0));
}

TEST(SeqNumUnwrapperTest, UnwrapBackwardsWithDivisor) {
    SeqNumUnwrapper<uint8_t, 33> unwrapper;
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(-2, unwrapper.UnwrapBackwards(31));
    EXPECT_EQ(-33, unwrapper.UnwrapBackwards(0));
}

// Number unwrapper tests
TEST(SequenceNumberUnwrapperTest, Limits) {
    SeqNumUnwrapper<uint16_t> unwrapper;

    EXPECT_EQ(0, unwrapper.Unwrap(0));
    // Forward
    EXPECT_EQ(0x8000, unwrapper.Unwrap(0x8000));
    // Delta is exactly 0x8000 but current is lower than input, wrap backwards.
    // Backward
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    // Forward
    EXPECT_EQ(0x8000, unwrapper.Unwrap(0x8000));
    // Forward
    EXPECT_EQ(0xFFFF, unwrapper.Unwrap(0xFFFF));
    // Forward
    EXPECT_EQ(0x10000, unwrapper.Unwrap(0));
    // Backward
    EXPECT_EQ(0xFFFF, unwrapper.Unwrap(0xFFFF));
    // Backward
    EXPECT_EQ(0x8000, unwrapper.Unwrap(0x8000));
    // Backward
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    // Backward, and will wrap backwards past zero.
    // Don't allow negative values.
    EXPECT_EQ(0xFFFF, unwrapper.Unwrap(0xFFFF));
}

TEST(SequenceNumberUnwrapperTest, ForwardWraps) {
    int64_t seq = 0;
    SeqNumUnwrapper<uint16_t> unwrapper;

    const int kMaxIncrease = 0x8000 - 1;
    const int kNumWraps = 4;
    for (int i = 0; i < kNumWraps * 2; ++i) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
        seq += kMaxIncrease;
    }

    unwrapper.set_last_unwrapped_value(0);
    for (int seq = 0; seq < kNumWraps/* * 0xFFFF*/; ++seq) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
    }
}

TEST(SequenceNumberUnwrapperTest, BackwardWraps) {
    SeqNumUnwrapper<uint16_t> unwrapper;

    const int kMaxDecrease = 0x8000 - 1;
    const int kNumWraps = 4;
    int64_t seq = kNumWraps * 2 * kMaxDecrease;
    unwrapper.set_last_unwrapped_value(seq);
    for (int i = kNumWraps * 2; i >= 0; --i) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
        seq -= kMaxDecrease;
    }

    seq = kNumWraps;//* 0xFFFF;
    unwrapper.set_last_unwrapped_value(seq);
    for (; seq >= 0; --seq) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
    }
}

TEST(TimestampUnwrapperTest, Limits) {
    SeqNumUnwrapper<uint32_t> unwrapper;

    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(0x80000000, unwrapper.Unwrap(0x80000000));
    // Delta is exactly 0x80000000 but current is lower than input, wrap
    // backwards.
    EXPECT_EQ(0, unwrapper.Unwrap(0));

    EXPECT_EQ(0x80000000, unwrapper.Unwrap(0x80000000));
    EXPECT_EQ(0xFFFFFFFF, unwrapper.Unwrap(0xFFFFFFFF));
    EXPECT_EQ(0x100000000, unwrapper.Unwrap(0x00000000));
    EXPECT_EQ(0xFFFFFFFF, unwrapper.Unwrap(0xFFFFFFFF));
    EXPECT_EQ(0x80000000, unwrapper.Unwrap(0x80000000));
    EXPECT_EQ(0, unwrapper.Unwrap(0));

    // Don't allow negative values.
    EXPECT_EQ(0xFFFFFFFF, unwrapper.Unwrap(0xFFFFFFFF));
}

TEST(TimestampUnwrapperTest, ForwardWraps) {
    int64_t ts = 0;
    SeqNumUnwrapper<uint32_t> unwrapper;

    const int64_t kMaxIncrease = 0x80000000 - 1;
    const int kNumWraps = 4;
    for (int i = 0; i < kNumWraps * 2; ++i) {
        int64_t unwrapped =
            unwrapper.Unwrap(static_cast<uint32_t>(ts & 0xFFFFFFFF));
        EXPECT_EQ(ts, unwrapped);
        ts += kMaxIncrease;
    }
}

TEST(TimestampUnwrapper, BackwardWraps) {
    SeqNumUnwrapper<uint32_t> unwrapper;

    const int64_t kMaxDecrease = 0x80000000 - 1;
    const int kNumWraps = 4;
    int64_t ts = kNumWraps * 2 * kMaxDecrease;
    unwrapper.set_last_unwrapped_value(ts);
    for (int i = 0; i <= kNumWraps * 2; ++i) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint32_t>(ts & 0xFFFFFFFF));
        EXPECT_EQ(ts, unwrapped);
        ts -= kMaxDecrease;
    }
}
    
} // namespace test
} // namespace naivertc
