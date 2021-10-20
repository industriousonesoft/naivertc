#include "rtc/rtp_rtcp/components/wrap_around_checker.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(RTP_RTCP_WrapAroundCheckerTest, IsNewerSequenceNumberEqual) {
    EXPECT_FALSE(IsNewerSequenceNumber(0x0001, 0x0001));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, IsNewerSequenceNumberNoWrap) {
    EXPECT_TRUE(IsNewerSequenceNumber(0xFFFF, 0xFFFE));
    EXPECT_TRUE(IsNewerSequenceNumber(0x0001, 0x0000));
    EXPECT_TRUE(IsNewerSequenceNumber(0x0100, 0x00FF));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, IsNewerSequenceNumberForwardWrap) {
    EXPECT_TRUE(IsNewerSequenceNumber(0x0000, 0xFFFF));
    EXPECT_TRUE(IsNewerSequenceNumber(0x0000, 0xFF00));
    EXPECT_TRUE(IsNewerSequenceNumber(0x00FF, 0xFFFF));
    EXPECT_TRUE(IsNewerSequenceNumber(0x00FF, 0xFF00));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, IsNewerSequenceNumberBackwardWrap) {
    EXPECT_FALSE(IsNewerSequenceNumber(0xFFFF, 0x0000));
    EXPECT_FALSE(IsNewerSequenceNumber(0xFF00, 0x0000));
    EXPECT_FALSE(IsNewerSequenceNumber(0xFFFF, 0x00FF));
    EXPECT_FALSE(IsNewerSequenceNumber(0xFF00, 0x00FF));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, IsNewerSequenceNumberHalfWayApart) {
    EXPECT_TRUE(IsNewerSequenceNumber(0x8000, 0x0000));
    EXPECT_FALSE(IsNewerSequenceNumber(0x0000, 0x8000));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, IsNewerTimestampEqual) {
    EXPECT_FALSE(IsNewerTimestamp(0x00000001, 0x000000001));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, IsNewerTimestampNoWrap) {
    EXPECT_TRUE(IsNewerTimestamp(0xFFFFFFFF, 0xFFFFFFFE));
    EXPECT_TRUE(IsNewerTimestamp(0x00000001, 0x00000000));
    EXPECT_TRUE(IsNewerTimestamp(0x00010000, 0x0000FFFF));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, IsNewerTimestampForwardWrap) {
    EXPECT_TRUE(IsNewerTimestamp(0x00000000, 0xFFFFFFFF));
    EXPECT_TRUE(IsNewerTimestamp(0x00000000, 0xFFFF0000));
    EXPECT_TRUE(IsNewerTimestamp(0x0000FFFF, 0xFFFFFFFF));
    EXPECT_TRUE(IsNewerTimestamp(0x0000FFFF, 0xFFFF0000));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, IsNewerTimestampBackwardWrap) {
    EXPECT_FALSE(IsNewerTimestamp(0xFFFFFFFF, 0x00000000));
    EXPECT_FALSE(IsNewerTimestamp(0xFFFF0000, 0x00000000));
    EXPECT_FALSE(IsNewerTimestamp(0xFFFFFFFF, 0x0000FFFF));
    EXPECT_FALSE(IsNewerTimestamp(0xFFFF0000, 0x0000FFFF));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, IsNewerTimestampHalfWayApart) {
    EXPECT_TRUE(IsNewerTimestamp(0x80000000, 0x00000000));
    EXPECT_FALSE(IsNewerTimestamp(0x00000000, 0x80000000));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, LatestSequenceNumberNoWrap) {
    EXPECT_EQ(0xFFFFu, LatestSequenceNumber(0xFFFF, 0xFFFE));
    EXPECT_EQ(0x0001u, LatestSequenceNumber(0x0001, 0x0000));
    EXPECT_EQ(0x0100u, LatestSequenceNumber(0x0100, 0x00FF));

    EXPECT_EQ(0xFFFFu, LatestSequenceNumber(0xFFFE, 0xFFFF));
    EXPECT_EQ(0x0001u, LatestSequenceNumber(0x0000, 0x0001));
    EXPECT_EQ(0x0100u, LatestSequenceNumber(0x00FF, 0x0100));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, LatestSequenceNumberWrap) {
    EXPECT_EQ(0x0000u, LatestSequenceNumber(0x0000, 0xFFFF));
    EXPECT_EQ(0x0000u, LatestSequenceNumber(0x0000, 0xFF00));
    EXPECT_EQ(0x00FFu, LatestSequenceNumber(0x00FF, 0xFFFF));
    EXPECT_EQ(0x00FFu, LatestSequenceNumber(0x00FF, 0xFF00));

    EXPECT_EQ(0x0000u, LatestSequenceNumber(0xFFFF, 0x0000));
    EXPECT_EQ(0x0000u, LatestSequenceNumber(0xFF00, 0x0000));
    EXPECT_EQ(0x00FFu, LatestSequenceNumber(0xFFFF, 0x00FF));
    EXPECT_EQ(0x00FFu, LatestSequenceNumber(0xFF00, 0x00FF));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, LatestTimestampNoWrap) {
    EXPECT_EQ(0xFFFFFFFFu, LatestTimestamp(0xFFFFFFFF, 0xFFFFFFFE));
    EXPECT_EQ(0x00000001u, LatestTimestamp(0x00000001, 0x00000000));
    EXPECT_EQ(0x00010000u, LatestTimestamp(0x00010000, 0x0000FFFF));
}

TEST(RTP_RTCP_WrapAroundCheckerTest, LatestTimestampWrap) {
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