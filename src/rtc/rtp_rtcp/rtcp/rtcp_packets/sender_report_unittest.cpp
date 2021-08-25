#include "rtc/rtp_rtcp/rtcp/rtcp_packets/sender_report.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {

namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
const NtpTime kNtp(0x11121418, 0x22242628);
const uint32_t kRtpTimestamp = 0x33343536;
const uint32_t kPacketCount = 0x44454647;
const uint32_t kOctetCount = 0x55565758;
const uint8_t kPacket[] = {0x80, 200,  0x00, 0x06, 0x12, 0x34, 0x56,
                           0x78, 0x11, 0x12, 0x14, 0x18, 0x22, 0x24,
                           0x26, 0x28, 0x33, 0x34, 0x35, 0x36, 0x44,
                           0x45, 0x46, 0x47, 0x55, 0x56, 0x57, 0x58};
}

TEST(RtcpSenderReportTest, CreateWithoutReportBlocks) {
    SenderReport sr;
    sr.set_sender_ssrc(kSenderSsrc);
    sr.set_ntp(kNtp);
    sr.set_rtp_timestamp(kRtpTimestamp);
    sr.set_sender_packet_count(kPacketCount);
    sr.set_sender_octet_count(kOctetCount);

    BinaryBuffer raw = sr.Build();
    EXPECT_THAT(raw, testing::ElementsAreArray(kPacket));
}

TEST(RtcpSenderReportTest, ParseWithoutReportBlocks) {
    SenderReport parsed;
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kPacket, sizeof(kPacket)));
    EXPECT_TRUE(parsed.Parse(common_header));

    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    EXPECT_EQ(kNtp, parsed.ntp());
    EXPECT_EQ(kRtpTimestamp, parsed.rtp_timestamp());
    EXPECT_EQ(kPacketCount, parsed.sender_packet_count());
    EXPECT_EQ(kOctetCount, parsed.sender_octet_count());
    EXPECT_TRUE(parsed.report_blocks().empty());

}

    
} // namespace test
} // namespace naivertc
