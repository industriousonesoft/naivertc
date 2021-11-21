#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

using namespace naivertc::wrap_around_utils;

namespace naivertc {
namespace test {

MY_TEST(WrapAroundUtilsTest, AheadOrAt) {
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

MY_TEST(WrapAroundUtilsTest, AheadOrAtWithDivisor) {
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

MY_TEST(WrapAroundUtilsTest, AheadOf) {
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

MY_TEST(WrapAroundUtilsTest, AheadOfWithDivisor) {
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

MY_TEST(WrapAroundUtilsTest, ForwardDiffWithDivisor) {
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

MY_TEST(WrapAroundUtilsTest, ReverseDiffWithDivisor) {
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

MY_TEST(WrapAroundUtilsTest, Comparator) {
    std::set<uint8_t, NewerThan<uint8_t>> seq_nums_asc;
    std::set<uint8_t, OlderThan<uint8_t>> seq_nums_desc;

    uint8_t x = 0;
    for (int i = 0; i < 128; ++i) {
        EXPECT_TRUE(seq_nums_asc.insert(x).second);
        EXPECT_TRUE(seq_nums_desc.insert(x).second);
        ASSERT_EQ(x, *seq_nums_asc.begin());
        ASSERT_EQ(x, *seq_nums_desc.rbegin());
        ++x;
    }

    seq_nums_asc.clear();
    seq_nums_desc.clear();
    x = 199;
    for (int i = 0; i < 128; ++i) {
        EXPECT_TRUE(seq_nums_asc.insert(x).second);
        EXPECT_TRUE(seq_nums_desc.insert(x).second);
        ASSERT_EQ(x, *seq_nums_asc.begin());
        ASSERT_EQ(x, *seq_nums_desc.rbegin());
        ++x;
    }
}

MY_TEST(WrapAroundUtilsTest, ComparatorWithDivisor) {
    const uint8_t D = 223;

    std::set<uint8_t, NewerThan<uint8_t, D>> seq_nums_asc;
    std::set<uint8_t, OlderThan<uint8_t, D>> seq_nums_desc;

    uint8_t x = 0;
    for (int i = 0; i < D / 2; ++i) {
        seq_nums_asc.insert(x);
        seq_nums_desc.insert(x);
        ASSERT_EQ(x, *seq_nums_asc.begin());
        ASSERT_EQ(x, *seq_nums_desc.rbegin());
        x = Add<D>(x, 1);
    }

    seq_nums_asc.clear();
    seq_nums_desc.clear();
    x = 200;
    for (int i = 0; i < D / 2; ++i) {
        seq_nums_asc.insert(x);
        seq_nums_desc.insert(x);
        ASSERT_EQ(x, *seq_nums_asc.begin());
        ASSERT_EQ(x, *seq_nums_desc.rbegin());
        x = Add<D>(x, 1);
    }
}

MY_TEST(WrapAroundUtilsTest, IsNewerSequenceNumberEqual) {
    EXPECT_FALSE(AheadOf<uint16_t>(0x0001, 0x0001));
}

MY_TEST(WrapAroundUtilsTest, IsNewerSequenceNumberNoWrap) {
    EXPECT_TRUE(AheadOf<uint16_t>(0xFFFF, 0xFFFE));
    EXPECT_TRUE(AheadOf<uint16_t>(0x0001, 0x0000));
    EXPECT_TRUE(AheadOf<uint16_t>(0x0100, 0x00FF));
}

MY_TEST(WrapAroundUtilsTest, IsNewerSequenceNumberForwardWrap) {
    EXPECT_TRUE(AheadOf<uint16_t>(0x0000, 0xFFFF));
    EXPECT_TRUE(AheadOf<uint16_t>(0x0000, 0xFF00));
    EXPECT_TRUE(AheadOf<uint16_t>(0x00FF, 0xFFFF));
    EXPECT_TRUE(AheadOf<uint16_t>(0x00FF, 0xFF00));
}

MY_TEST(WrapAroundUtilsTest, IsNewerSequenceNumberBackwardWrap) {
    EXPECT_FALSE(AheadOf<uint16_t>(0xFFFF, 0x0000));
    EXPECT_FALSE(AheadOf<uint16_t>(0xFF00, 0x0000));
    EXPECT_FALSE(AheadOf<uint16_t>(0xFFFF, 0x00FF));
    EXPECT_FALSE(AheadOf<uint16_t>(0xFF00, 0x00FF));
}

MY_TEST(WrapAroundUtilsTest, IsNewerSequenceNumberHalfWayApart) {
    EXPECT_TRUE(AheadOf<uint16_t>(0x8000, 0x0000));
    EXPECT_FALSE(AheadOf<uint16_t>(0x0000, 0x8000));
}

MY_TEST(WrapAroundUtilsTest, IsNewerTimestampEqual) {
    EXPECT_FALSE(AheadOf<uint32_t>(0x00000001, 0x000000001));
}

MY_TEST(WrapAroundUtilsTest, IsNewerTimestampNoWrap) {
    EXPECT_TRUE(AheadOf<uint32_t>(0xFFFFFFFF, 0xFFFFFFFE));
    EXPECT_TRUE(AheadOf<uint32_t>(0x00000001, 0x00000000));
    EXPECT_TRUE(AheadOf<uint32_t>(0x00010000, 0x0000FFFF));
}

MY_TEST(WrapAroundUtilsTest, IsNewerTimestampForwardWrap) {
    EXPECT_TRUE(AheadOf<uint32_t>(0x00000000, 0xFFFFFFFF));
    EXPECT_TRUE(AheadOf<uint32_t>(0x00000000, 0xFFFF0000));
    EXPECT_TRUE(AheadOf<uint32_t>(0x0000FFFF, 0xFFFFFFFF));
    EXPECT_TRUE(AheadOf<uint32_t>(0x0000FFFF, 0xFFFF0000));
}

MY_TEST(WrapAroundUtilsTest, IsNewerTimestampBackwardWrap) {
    EXPECT_FALSE(AheadOf<uint32_t>(0xFFFFFFFF, 0x00000000));
    EXPECT_FALSE(AheadOf<uint32_t>(0xFFFF0000, 0x00000000));
    EXPECT_FALSE(AheadOf<uint32_t>(0xFFFFFFFF, 0x0000FFFF));
    EXPECT_FALSE(AheadOf<uint32_t>(0xFFFF0000, 0x0000FFFF));
}

MY_TEST(WrapAroundUtilsTest, IsNewerTimestampHalfWayApart) {
    EXPECT_TRUE(AheadOf<uint32_t>(0x80000000, 0x00000000));
    EXPECT_FALSE(AheadOf<uint32_t>(0x00000000, 0x80000000));
}

MY_TEST(WrapAroundUtilsTest, LatestSequenceNumberNoWrap) {
    EXPECT_EQ(0xFFFFu, Latest<uint16_t>(0xFFFF, 0xFFFE));
    EXPECT_EQ(0x0001u, Latest<uint16_t>(0x0001, 0x0000));
    EXPECT_EQ(0x0100u, Latest<uint16_t>(0x0100, 0x00FF));

    EXPECT_EQ(0xFFFFu, Latest<uint16_t>(0xFFFE, 0xFFFF));
    EXPECT_EQ(0x0001u, Latest<uint16_t>(0x0000, 0x0001));
    EXPECT_EQ(0x0100u, Latest<uint16_t>(0x00FF, 0x0100));
}

MY_TEST(WrapAroundUtilsTest, LatestSequenceNumberWrap) {
    EXPECT_EQ(0x0000u, Latest<uint16_t>(0x0000, 0xFFFF));
    EXPECT_EQ(0x0000u, Latest<uint16_t>(0x0000, 0xFF00));
    EXPECT_EQ(0x00FFu, Latest<uint16_t>(0x00FF, 0xFFFF));
    EXPECT_EQ(0x00FFu, Latest<uint16_t>(0x00FF, 0xFF00));

    EXPECT_EQ(0x0000u, Latest<uint16_t>(0xFFFF, 0x0000));
    EXPECT_EQ(0x0000u, Latest<uint16_t>(0xFF00, 0x0000));
    EXPECT_EQ(0x00FFu, Latest<uint16_t>(0xFFFF, 0x00FF));
    EXPECT_EQ(0x00FFu, Latest<uint16_t>(0xFF00, 0x00FF));
}

MY_TEST(WrapAroundUtilsTest, LatestTimestampNoWrap) {
    EXPECT_EQ(0xFFFFFFFFu, Latest<uint32_t>(0xFFFFFFFF, 0xFFFFFFFE));
    EXPECT_EQ(0x00000001u, Latest<uint32_t>(0x00000001, 0x00000000));
    EXPECT_EQ(0x00010000u, Latest<uint32_t>(0x00010000, 0x0000FFFF));
}

MY_TEST(WrapAroundUtilsTest, LatestTimestampWrap) {
    EXPECT_EQ(0x00000000u, Latest<uint32_t>(0x00000000, 0xFFFFFFFF));
    EXPECT_EQ(0x00000000u, Latest<uint32_t>(0x00000000, 0xFFFF0000));
    EXPECT_EQ(0x0000FFFFu, Latest<uint32_t>(0x0000FFFF, 0xFFFFFFFF));
    EXPECT_EQ(0x0000FFFFu, Latest<uint32_t>(0x0000FFFF, 0xFFFF0000));

    EXPECT_EQ(0x00000000u, Latest<uint32_t>(0xFFFFFFFF, 0x00000000));
    EXPECT_EQ(0x00000000u, Latest<uint32_t>(0xFFFF0000, 0x00000000));
    EXPECT_EQ(0x0000FFFFu, Latest<uint32_t>(0xFFFFFFFF, 0x0000FFFF));
    EXPECT_EQ(0x0000FFFFu, Latest<uint32_t>(0xFFFF0000, 0x0000FFFF));
}
    
} // namespace test
} // namespace naivertc

