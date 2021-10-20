#include "rtc/rtp_rtcp/rtcp/rtcp_nack_stats.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(RTP_RTCP_RtcpNackStatsTest, Requests) {
    RtcpNackStats stats;
    EXPECT_EQ(0U, stats.unique_requests());
    EXPECT_EQ(0U, stats.requests());
    stats.ReportRequest(10);
    EXPECT_EQ(1U, stats.unique_requests());
    EXPECT_EQ(1U, stats.requests());

    stats.ReportRequest(10);
    EXPECT_EQ(1U, stats.unique_requests());
    stats.ReportRequest(11);
    EXPECT_EQ(2U, stats.unique_requests());

    stats.ReportRequest(11);
    EXPECT_EQ(2U, stats.unique_requests());
    stats.ReportRequest(13);
    EXPECT_EQ(3U, stats.unique_requests());

    stats.ReportRequest(11);
    EXPECT_EQ(3U, stats.unique_requests());
    EXPECT_EQ(6U, stats.requests());
}

TEST(RTP_RTCP_RtcpNackStatsTest, RequestsWithWrap) {
    RtcpNackStats stats;
    stats.ReportRequest(65534);
    EXPECT_EQ(1U, stats.unique_requests());

    stats.ReportRequest(65534);
    EXPECT_EQ(1U, stats.unique_requests());
    stats.ReportRequest(65535);
    EXPECT_EQ(2U, stats.unique_requests());

    stats.ReportRequest(65535);
    EXPECT_EQ(2U, stats.unique_requests());
    stats.ReportRequest(0);
    EXPECT_EQ(3U, stats.unique_requests());

    stats.ReportRequest(65535);
    EXPECT_EQ(3U, stats.unique_requests());
    stats.ReportRequest(0);
    EXPECT_EQ(3U, stats.unique_requests());
    stats.ReportRequest(1);
    EXPECT_EQ(4U, stats.unique_requests());
    EXPECT_EQ(8U, stats.requests());
}

} // namespace test
} // namespace naivertc