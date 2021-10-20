#include "rtc/rtp_rtcp/components/number_unwrapper.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(RTP_RTCP_SequenceNumberUnwrapperTest, Limits) {
    SequenceNumberUnwrapper unwrapper;

    EXPECT_EQ(0, unwrapper.Unwrap(0));
    EXPECT_EQ(0x8000, unwrapper.Unwrap(0x8000));
    // Delta is exactly 0x8000 but current is lower than input, wrap backwards.
    EXPECT_EQ(0, unwrapper.Unwrap(0));

    EXPECT_EQ(0x8000, unwrapper.Unwrap(0x8000));
    EXPECT_EQ(0xFFFF, unwrapper.Unwrap(0xFFFF));
    EXPECT_EQ(0x10000, unwrapper.Unwrap(0));
    EXPECT_EQ(0xFFFF, unwrapper.Unwrap(0xFFFF));
    EXPECT_EQ(0x8000, unwrapper.Unwrap(0x8000));
    EXPECT_EQ(0, unwrapper.Unwrap(0));

    // Don't allow negative values.
    EXPECT_EQ(0xFFFF, unwrapper.Unwrap(0xFFFF));
}

TEST(RTP_RTCP_SequenceNumberUnwrapperTest, ForwardWraps) {
    int64_t seq = 0;
    SequenceNumberUnwrapper unwrapper;

    const int kMaxIncrease = 0x8000 - 1;
    const int kNumWraps = 4;
    for (int i = 0; i < kNumWraps * 2; ++i) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
        seq += kMaxIncrease;
    }

    unwrapper.UpdateLast(0);
    for (int seq = 0; seq < kNumWraps * 0xFFFF; ++seq) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
    }
}

TEST(RTP_RTCP_SequenceNumberUnwrapperTest, BackwardWraps) {
    SequenceNumberUnwrapper unwrapper;

    const int kMaxDecrease = 0x8000 - 1;
    const int kNumWraps = 4;
    int64_t seq = kNumWraps * 2 * kMaxDecrease;
    unwrapper.UpdateLast(seq);
    for (int i = kNumWraps * 2; i >= 0; --i) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
        seq -= kMaxDecrease;
    }

    seq = kNumWraps * 0xFFFF;
    unwrapper.UpdateLast(seq);
    for (; seq >= 0; --seq) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint16_t>(seq & 0xFFFF));
        EXPECT_EQ(seq, unwrapped);
    }
}

TEST(RTP_RTCP_TimestampUnwrapperTest, Limits) {
    TimestampUnwrapper unwrapper;

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

TEST(RTP_RTCP_TimestampUnwrapperTest, ForwardWraps) {
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

TEST(RTP_RTCP_TimestampUnwrapper, BackwardWraps) {
    TimestampUnwrapper unwrapper;

    const int64_t kMaxDecrease = 0x80000000 - 1;
    const int kNumWraps = 4;
    int64_t ts = kNumWraps * 2 * kMaxDecrease;
    unwrapper.UpdateLast(ts);
    for (int i = 0; i <= kNumWraps * 2; ++i) {
        int64_t unwrapped = unwrapper.Unwrap(static_cast<uint32_t>(ts & 0xFFFFFFFF));
        EXPECT_EQ(ts, unwrapped);
        ts -= kMaxDecrease;
    }
}

} // namespace test
} // namespace naivertc