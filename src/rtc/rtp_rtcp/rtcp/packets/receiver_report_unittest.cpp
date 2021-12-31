#include "rtc/rtp_rtcp/rtcp/packets/receiver_report.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/common_header.hpp"
#include "common/array_view.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {

namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
const uint8_t kFractionLost = 55;
const int32_t kCumulativeLost = 0x111213;
const uint32_t kExtHighestSeqNum = 0x22232425;
const uint32_t kJitter = 0x33343536;
const uint32_t kLastSr = 0x44454647;
const uint32_t kDelayLastSr = 0x55565758;
// Manually created ReceiverReport with one ReportBlock matching constants
// above.
// Having this block allows to test Create and Parse separately.
const uint8_t kPacket[] = {0x81, 201,  0x00, 0x07, 0x12, 0x34, 0x56, 0x78,
                           0x23, 0x45, 0x67, 0x89, 55,   0x11, 0x12, 0x13,
                           0x22, 0x23, 0x24, 0x25, 0x33, 0x34, 0x35, 0x36,
                           0x44, 0x45, 0x46, 0x47, 0x55, 0x56, 0x57, 0x58};
} // namespace

MY_TEST(RtcpReceiverReportTest, ParseWithOneReportBlock) {
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kPacket, sizeof(kPacket)));
    EXPECT_EQ(201, common_header.type());
    EXPECT_EQ(sizeof(kPacket) - 4, common_header.payload_size());
    EXPECT_EQ(1u, common_header.count());

    ReceiverReport rr;
    EXPECT_TRUE(rr.Parse(common_header));
    EXPECT_EQ(1u, rr.report_blocks().size());
    EXPECT_EQ(kSenderSsrc, rr.sender_ssrc());
    EXPECT_EQ(kRemoteSsrc, rr.report_blocks()[0].source_ssrc());
    EXPECT_EQ(kFractionLost, rr.report_blocks()[0].fraction_lost());
    EXPECT_EQ(kCumulativeLost, rr.report_blocks()[0].cumulative_packet_lost());
    EXPECT_EQ(0x2223, rr.report_blocks()[0].sequence_num_cycles());
    EXPECT_EQ(0x2425, rr.report_blocks()[0].highest_seq_num());
    EXPECT_EQ(kJitter, rr.report_blocks()[0].jitter());
    EXPECT_EQ(kLastSr, rr.report_blocks()[0].last_sr_ntp_timestamp());
    EXPECT_EQ(kDelayLastSr, rr.report_blocks()[0].delay_since_last_sr());
    
}

MY_TEST(RtcpReceiverReportTest, CreateWithOneReportBlock) {
    ReceiverReport rr;
    rr.set_sender_ssrc(kSenderSsrc);
    ReportBlock rb;
    rb.set_media_ssrc(kRemoteSsrc);
    rb.set_fraction_lost(kFractionLost);
    rb.set_cumulative_packet_lost(kCumulativeLost);
    rb.set_extended_highest_sequence_num(kExtHighestSeqNum);
    rb.set_jitter(kJitter);
    rb.set_last_sr_ntp_timestamp(kLastSr);
    rb.set_delay_sr_since_last_sr(kDelayLastSr);
    rr.AddReportBlock(rb);

    auto packet = rr.Build();
    EXPECT_THAT(ArrayView<const uint8_t>(packet), testing::ElementsAreArray(kPacket));
}
    
} // namespace test
} // namespace naivertc
