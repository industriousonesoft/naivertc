#include "rtc/rtp_rtcp/components/num_unwrapper.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(NumberUnwrapperTest, SeqNumLimits) {
    SeqNumUnwrapper unwrapper;

    EXPECT_EQ(0, unwrapper.Unwrap(0));
    // Forward
    EXPECT_EQ(0x8000, unwrapper.Unwrap(0x8000));
    // Backward
    // Delta is exactly 0x8000 but current is lower than input, wrap backwards.
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

    // Don't allow negative values.
    EXPECT_EQ(0xFFFF, unwrapper.Unwrap(0xFFFF));
}

MY_TEST(NumberUnwrapperTest, SeqNumForwardWraps) {
    int64_t seq = 0;
    SeqNumUnwrapper unwrapper;

    const int kMaxIncrease = 0x8000 - 1;
    const int kNumWraps = 4;
    for (int i = 0; i < kNumWraps * 2; ++i) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
        seq += kMaxIncrease;
    }

    unwrapper.set_last_unwrapped_value(0);
    for (int seq = 0; seq < kNumWraps * 0xFFFF; ++seq) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
    }
}

MY_TEST(NumberUnwrapperTest, SeqNumBackwardWraps) {
    SeqNumUnwrapper unwrapper;

    const int kMaxDecrease = 0x8000 - 1;
    const int kNumWraps = 4;
    int64_t seq = kNumWraps * 2 * kMaxDecrease;
    unwrapper.set_last_unwrapped_value(seq);
    for (int i = kNumWraps * 2; i >= 0; --i) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
        seq -= kMaxDecrease;
    }

    seq = kNumWraps * 0xFFFF;
    unwrapper.set_last_unwrapped_value(seq);
    for (; seq >= 0; --seq) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
    }
}

MY_TEST(NumberUnwrapperTest, TimestampLimits) {
    TimestampUnwrapper unwrapper;

    EXPECT_EQ(0, unwrapper.Unwrap(0));
    // Forward
    EXPECT_EQ(0x80000000, unwrapper.Unwrap(0x80000000));
    // Backward
    // Delta is exactly 0x80000000 but current is lower than input, wrap
    // backwards.
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    // Forward
    EXPECT_EQ(0x80000000, unwrapper.Unwrap(0x80000000));
    // Forward
    EXPECT_EQ(0xFFFFFFFF, unwrapper.Unwrap(0xFFFFFFFF));
    // Forward
    EXPECT_EQ(0x100000000, unwrapper.Unwrap(0x00000000));
    // Backward
    EXPECT_EQ(0xFFFFFFFF, unwrapper.Unwrap(0xFFFFFFFF));
    // Backward
    EXPECT_EQ(0x80000000, unwrapper.Unwrap(0x80000000));
    // Backward
    EXPECT_EQ(0, unwrapper.Unwrap(0));
    // Backward
    // Don't allow negative values.
    EXPECT_EQ(0xFFFFFFFF, unwrapper.Unwrap(0xFFFFFFFF));
}

MY_TEST(NumberUnwrapperTest, TimestampForwardWraps) {
    int64_t ts = 0;
    TimestampUnwrapper unwrapper;

    const int64_t kMaxIncrease = 0x80000000 - 1;
    const int kNumWraps = 4;
    for (int i = 0; i < kNumWraps * 2; ++i) {
        int64_t unwrapped =
            unwrapper.Unwrap(static_cast<uint32_t>(ts & 0xFFFFFFFF));
        EXPECT_EQ(ts, unwrapped);
        ts += kMaxIncrease;
    }
}

MY_TEST(NumberUnwrapperTest, TimestampBackwardWraps) {
    TimestampUnwrapper unwrapper;

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