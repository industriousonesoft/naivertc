#include "rtc/rtp_rtcp/components/wrap_around_checker.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(WrapAroundCheckerTest, IsNewerSequenceNumberEqual) {
    EXPECT_FALSE(IsNewerSequenceNumber(0x0001, 0x0001));
}

MY_TEST(WrapAroundCheckerTest, IsNewerSequenceNumberNoWrap) {
    EXPECT_TRUE(IsNewerSequenceNumber(0xFFFF, 0xFFFE));
    EXPECT_TRUE(IsNewerSequenceNumber(0x0001, 0x0000));
    EXPECT_TRUE(IsNewerSequenceNumber(0x0100, 0x00FF));
}

MY_TEST(WrapAroundCheckerTest, IsNewerSequenceNumberForwardWrap) {
    EXPECT_TRUE(IsNewerSequenceNumber(0x0000, 0xFFFF));
    EXPECT_TRUE(IsNewerSequenceNumber(0x0000, 0xFF00));
    EXPECT_TRUE(IsNewerSequenceNumber(0x00FF, 0xFFFF));
    EXPECT_TRUE(IsNewerSequenceNumber(0x00FF, 0xFF00));
}

MY_TEST(WrapAroundCheckerTest, IsNewerSequenceNumberBackwardWrap) {
    EXPECT_FALSE(IsNewerSequenceNumber(0xFFFF, 0x0000));
    EXPECT_FALSE(IsNewerSequenceNumber(0xFF00, 0x0000));
    EXPECT_FALSE(IsNewerSequenceNumber(0xFFFF, 0x00FF));
    EXPECT_FALSE(IsNewerSequenceNumber(0xFF00, 0x00FF));
}

MY_TEST(WrapAroundCheckerTest, IsNewerSequenceNumberHalfWayApart) {
    EXPECT_TRUE(IsNewerSequenceNumber(0x8000, 0x0000));
    EXPECT_FALSE(IsNewerSequenceNumber(0x0000, 0x8000));
}

MY_TEST(WrapAroundCheckerTest, IsNewerTimestampEqual) {
    EXPECT_FALSE(IsNewerTimestamp(0x00000001, 0x000000001));
}

MY_TEST(WrapAroundCheckerTest, IsNewerTimestampNoWrap) {
    EXPECT_TRUE(IsNewerTimestamp(0xFFFFFFFF, 0xFFFFFFFE));
    EXPECT_TRUE(IsNewerTimestamp(0x00000001, 0x00000000));
    EXPECT_TRUE(IsNewerTimestamp(0x00010000, 0x0000FFFF));
}

MY_TEST(WrapAroundCheckerTest, IsNewerTimestampForwardWrap) {
    EXPECT_TRUE(IsNewerTimestamp(0x00000000, 0xFFFFFFFF));
    EXPECT_TRUE(IsNewerTimestamp(0x00000000, 0xFFFF0000));
    EXPECT_TRUE(IsNewerTimestamp(0x0000FFFF, 0xFFFFFFFF));
    EXPECT_TRUE(IsNewerTimestamp(0x0000FFFF, 0xFFFF0000));
}

MY_TEST(WrapAroundCheckerTest, IsNewerTimestampBackwardWrap) {
    EXPECT_FALSE(IsNewerTimestamp(0xFFFFFFFF, 0x00000000));
    EXPECT_FALSE(IsNewerTimestamp(0xFFFF0000, 0x00000000));
    EXPECT_FALSE(IsNewerTimestamp(0xFFFFFFFF, 0x0000FFFF));
    EXPECT_FALSE(IsNewerTimestamp(0xFFFF0000, 0x0000FFFF));
}

MY_TEST(WrapAroundCheckerTest, IsNewerTimestampHalfWayApart) {
    EXPECT_TRUE(IsNewerTimestamp(0x80000000, 0x00000000));
    EXPECT_FALSE(IsNewerTimestamp(0x00000000, 0x80000000));
}

MY_TEST(WrapAroundCheckerTest, LatestSequenceNumberNoWrap) {
    EXPECT_EQ(0xFFFFu, LatestSequenceNumber(0xFFFF, 0xFFFE));
    EXPECT_EQ(0x0001u, LatestSequenceNumber(0x0001, 0x0000));
    EXPECT_EQ(0x0100u, LatestSequenceNumber(0x0100, 0x00FF));

    EXPECT_EQ(0xFFFFu, LatestSequenceNumber(0xFFFE, 0xFFFF));
    EXPECT_EQ(0x0001u, LatestSequenceNumber(0x0000, 0x0001));
    EXPECT_EQ(0x0100u, LatestSequenceNumber(0x00FF, 0x0100));
}

MY_TEST(WrapAroundCheckerTest, LatestSequenceNumberWrap) {
    EXPECT_EQ(0x0000u, LatestSequenceNumber(0x0000, 0xFFFF));
    EXPECT_EQ(0x0000u, LatestSequenceNumber(0x0000, 0xFF00));
    EXPECT_EQ(0x00FFu, LatestSequenceNumber(0x00FF, 0xFFFF));
    EXPECT_EQ(0x00FFu, LatestSequenceNumber(0x00FF, 0xFF00));

    EXPECT_EQ(0x0000u, LatestSequenceNumber(0xFFFF, 0x0000));
    EXPECT_EQ(0x0000u, LatestSequenceNumber(0xFF00, 0x0000));
    EXPECT_EQ(0x00FFu, LatestSequenceNumber(0xFFFF, 0x00FF));
    EXPECT_EQ(0x00FFu, LatestSequenceNumber(0xFF00, 0x00FF));
}

MY_TEST(WrapAroundCheckerTest, LatestTimestampNoWrap) {
    EXPECT_EQ(0xFFFFFFFFu, LatestTimestamp(0xFFFFFFFF, 0xFFFFFFFE));
    EXPECT_EQ(0x00000001u, LatestTimestamp(0x00000001, 0x00000000));
    EXPECT_EQ(0x00010000u, LatestTimestamp(0x00010000, 0x0000FFFF));
}

MY_TEST(WrapAroundCheckerTest, LatestTimestampWrap) {
    EXPECT_EQ(0x00000000u, LatestTimestamp(0x00000000, 0xFFFFFFFF));
    EXPECT_EQ(0x00000000u, LatestTimestamp(0x00000000, 0xFFFF0000));
    EXPECT_EQ(0x0000FFFFu, LatestTimestamp(0x0000FFFF, 0xFFFFFFFF));
    EXPECT_EQ(0x0000FFFFu, LatestTimestamp(0x0000FFFF, 0xFFFF0000));

    EXPECT_EQ(0x00000000u, LatestTimestamp(0xFFFFFFFF, 0x00000000));
    EXPECT_EQ(0x00000000u, LatestTimestamp(0xFFFF0000, 0x00000000));
    EXPECT_EQ(0x0000FFFFu, LatestTimestamp(0xFFFFFFFF, 0x0000FFFF));
    EXPECT_EQ(0x0000FFFFu, LatestTimestamp(0xFFFF0000, 0x0000FFFF));
}


} // namespace test
} // namespace naivertc