#include "rtc/rtp_rtcp/components/seq_num_utils.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(SeqNumUtilsTest, AheadOrAt) {
    uint8_t x = 0;
    uint8_t y = 0;
    ASSERT_TRUE(AheadOrAt(x, y));
    x++;
    ASSERT_TRUE(AheadOrAt(x, y));
    ASSERT_FALSE(AheadOrAt(y, x));

    for (int i = 0; i < 256; ++i) {
        ASSERT_TRUE(AheadOrAt(x, y));
        ++x;
        ++y;
    }

    x = 128;
    y = 0;
    ASSERT_TRUE(AheadOrAt(x, y));
    ASSERT_FALSE(AheadOrAt(y, x));

    x = 129;
    ASSERT_FALSE(AheadOrAt(x, y));
    ASSERT_TRUE(AheadOrAt(y, x));
    ASSERT_TRUE(AheadOrAt<uint16_t>(x, y));
    ASSERT_FALSE(AheadOrAt<uint16_t>(y, x));
}

TEST(SeqNumUtilsTest, AheadOrAtWithDivisor) {
    ASSERT_TRUE((AheadOrAt<uint8_t, 11>(5, 0)));
    ASSERT_FALSE((AheadOrAt<uint8_t, 11>(6, 0)));
    ASSERT_FALSE((AheadOrAt<uint8_t, 11>(0, 5)));
    ASSERT_TRUE((AheadOrAt<uint8_t, 11>(0, 6)));

    ASSERT_TRUE((AheadOrAt<uint8_t, 10>(5, 0)));
    ASSERT_FALSE((AheadOrAt<uint8_t, 10>(6, 0)));
    ASSERT_FALSE((AheadOrAt<uint8_t, 10>(0, 5)));
    ASSERT_TRUE((AheadOrAt<uint8_t, 10>(0, 6)));

    const uint8_t D = 211;
    uint8_t x = 0;
    for (int i = 0; i < D; ++i) {
        uint8_t next_x = Add<D>(x, 1);
        ASSERT_TRUE((AheadOrAt<uint8_t, D>(i, i)));
        ASSERT_TRUE((AheadOrAt<uint8_t, D>(next_x, i)));
        ASSERT_FALSE((AheadOrAt<uint8_t, D>(i, next_x)));
        x = next_x;
    }
}

TEST(SeqNumUtilsTest, AheadOf) {
    uint8_t x = 0;
    uint8_t y = 0;
    ASSERT_FALSE(AheadOf(x, y));
    ++x;
    ASSERT_TRUE(AheadOf(x, y));
    ASSERT_FALSE(AheadOf(y, x));
    for (int i = 0; i < 256; ++i) {
        ASSERT_TRUE(AheadOf(x, y));
        ++x;
        ++y;
    }

    x = 128;
    y = 0;
    for (int i = 0; i < 128; ++i) {
        ASSERT_TRUE(AheadOf(x, y));
        ASSERT_FALSE(AheadOf(y, x));
        x++;
        y++;
    }

    for (int i = 0; i < 128; ++i) {
        ASSERT_FALSE(AheadOf(x, y));
        ASSERT_TRUE(AheadOf(y, x));
        x++;
        y++;
    }

    x = 129;
    y = 0;
    ASSERT_FALSE(AheadOf(x, y));
    ASSERT_TRUE(AheadOf(y, x));
    ASSERT_TRUE(AheadOf<uint16_t>(x, y));
    ASSERT_FALSE(AheadOf<uint16_t>(y, x));
}

TEST(SeqNumUtilsTest, AheadOfWithDivisor) {
    ASSERT_TRUE((AheadOf<uint8_t, 11>(5, 0)));
    ASSERT_FALSE((AheadOf<uint8_t, 11>(6, 0)));
    ASSERT_FALSE((AheadOf<uint8_t, 11>(0, 5)));
    ASSERT_TRUE((AheadOf<uint8_t, 11>(0, 6)));

    ASSERT_TRUE((AheadOf<uint8_t, 10>(5, 0)));
    ASSERT_FALSE((AheadOf<uint8_t, 10>(6, 0)));
    ASSERT_FALSE((AheadOf<uint8_t, 10>(0, 5)));
    ASSERT_TRUE((AheadOf<uint8_t, 10>(0, 6)));

    const uint8_t D = 211;
    uint8_t x = 0;
    for (int i = 0; i < D; ++i) {
        uint8_t next_x = Add<D>(x, 1);
        ASSERT_FALSE((AheadOf<uint8_t, D>(i, i)));
        ASSERT_TRUE((AheadOf<uint8_t, D>(next_x, i)));
        ASSERT_FALSE((AheadOf<uint8_t, D>(i, next_x)));
        x = next_x;
    }
}

TEST(SeqNumUtilsTest, ForwardDiffWithDivisor) {
    const uint8_t kDivisor = 211;

    for (uint8_t i = 0; i < kDivisor - 1; ++i) {
        ASSERT_EQ(0, (ForwardDiff<uint8_t, kDivisor>(i, i)));
        ASSERT_EQ(1, (ForwardDiff<uint8_t, kDivisor>(i, i + 1)));
        ASSERT_EQ(kDivisor - 1, (ForwardDiff<uint8_t, kDivisor>(i + 1, i)));
    }

    for (uint8_t i = 1; i < kDivisor; ++i) {
        ASSERT_EQ(i, (ForwardDiff<uint8_t, kDivisor>(0, i)));
        ASSERT_EQ(kDivisor - i, (ForwardDiff<uint8_t, kDivisor>(i, 0)));
    }
}

TEST(SeqNumUtilsTest, ReverseDiffWithDivisor) {
    const uint8_t kDivisor = 241;

    for (uint8_t i = 0; i < kDivisor - 1; ++i) {
        ASSERT_EQ(0, (ReverseDiff<uint8_t, kDivisor>(i, i)));
        ASSERT_EQ(kDivisor - 1, (ReverseDiff<uint8_t, kDivisor>(i, i + 1)));
        ASSERT_EQ(1, (ReverseDiff<uint8_t, kDivisor>(i + 1, i)));
    }

    for (uint8_t i = 1; i < kDivisor; ++i) {
        ASSERT_EQ(kDivisor - i, (ReverseDiff<uint8_t, kDivisor>(0, i)));
        ASSERT_EQ(i, (ReverseDiff<uint8_t, kDivisor>(i, 0)));
    }
}
    
} // namespace test
} // namespace naivertc

