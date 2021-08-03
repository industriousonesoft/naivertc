#include "rtc/rtp_rtcp/rtcp_packets/report_block.hpp"

#include <gtest/gtest.h>

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {

constexpr uint32_t kRemoteSsrc = 0x1EF834FF;
constexpr uint8_t kFractionLost = 33;
constexpr int32_t kCumulativePacketLost = 0x4F56D3;
constexpr uint16_t kSeqNumCycles = 0x34D1;
constexpr uint16_t kHighestSeqNum = 0x78A9;
constexpr uint32_t kExtendedHighestSeqNum = 0x34D178A9;
constexpr uint32_t kJitter = 0x4F6D73A2;
constexpr uint32_t kLastSrNtpTimestamp = 0x01FF3467;
constexpr uint32_t kDelaySinceLastSr = 0x89D67F50;

TEST(RtcpReportBlockTest, ParseMatchPack) {
    ReportBlock rb;
    rb.set_media_ssrc(kRemoteSsrc);
    rb.set_fraction_lost(kFractionLost);
    rb.set_cumulative_packet_lost(kCumulativePacketLost);
    // rb.set_seq_num_cycles(kSeqNumCycles);
    // rb.set_highest_sequence_num(kHighestSeqNum);
    rb.set_extended_highest_sequence_num(kExtendedHighestSeqNum);
    rb.set_jitter(kJitter);
    rb.set_last_sr_ntp_timestamp(kLastSrNtpTimestamp);
    rb.set_delay_sr_since_last_sr(kDelaySinceLastSr);

    size_t buffer_size = ReportBlock::kFixedReportBlockSize;
    uint8_t buffer[buffer_size];
    rb.PackInto(buffer, buffer_size);

    ReportBlock parsed_rb;
    EXPECT_TRUE(parsed_rb.Parse(buffer, buffer_size));

    EXPECT_EQ(parsed_rb.source_ssrc(), kRemoteSsrc);
    EXPECT_EQ(parsed_rb.fraction_lost(), kFractionLost);
    EXPECT_EQ(parsed_rb.cumulative_packet_lost(), kCumulativePacketLost);
    EXPECT_EQ(parsed_rb.sequence_num_cycles(), kSeqNumCycles);
    EXPECT_EQ(parsed_rb.highest_seq_num(), kHighestSeqNum);
    EXPECT_EQ(parsed_rb.jitter(), kJitter);
    EXPECT_EQ(parsed_rb.last_sr_ntp_timestamp(), kLastSrNtpTimestamp);
    EXPECT_EQ(parsed_rb.delay_since_last_sr(), kDelaySinceLastSr);
}

} // namespace test
} // namespace naivertc
