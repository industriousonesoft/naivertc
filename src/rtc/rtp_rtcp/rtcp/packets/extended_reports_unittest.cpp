#include "rtc/rtp_rtcp/rtcp/packets/extended_reports.hpp"
#include "common/utils_random.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet_parser.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::make_tuple;
using ::testing::SizeIs;
using namespace naivertc::rtcp;

namespace naivertc {

namespace rtcp {

bool operator==(const Rrtr& lhs, const Rrtr& rhs) {
    return lhs.ntp() == rhs.ntp();
}

bool operator==(const Dlrr::SubBlock& lhs, const Dlrr::SubBlock& rhs) {
    return lhs.ssrc == rhs.ssrc &&
           lhs.last_rr == rhs.last_rr &&
           lhs.delay_since_last_rr == rhs.delay_since_last_rr;
}
    
} // namespace rtcp

namespace test {
namespace {

constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint8_t kPacketWithoutBlocks[] = {0x80, 207, 0x00, 0x01,
                                            0x12, 0x34, 0x56, 0x78};

// clang-format off
const uint8_t kTargetBitrateBlock[] = {TargetBitrate::kBlockType,   // Block ID.
                                            0x00,                   // Reserved.
                                                    0x00, 0x04,     // Length = 4 words.
                                        0x00, 0x01, 0x02, 0x03,     // S0T0 0x010203 kbps.
                                        0x01, 0x02, 0x03, 0x04,     // S0T1 0x020304 kbps.
                                        0x10, 0x03, 0x04, 0x05,     // S1T0 0x030405 kbps.
                                        0x11, 0x04, 0x05, 0x06 };   // S1T1 0x040506 kbps.
constexpr size_t kTargetBitrateBlockSize = sizeof(kTargetBitrateBlock);
// clang-format on

void Verify(const TargetBitrate::BitrateItem& expected, 
            const TargetBitrate::BitrateItem& actual) {
    EXPECT_EQ(expected.spatial_layer, actual.spatial_layer);
    EXPECT_EQ(expected.temporal_layer, actual.temporal_layer);
    EXPECT_EQ(expected.target_bitrate_kbps, actual.target_bitrate_kbps);
}

void Verify(const std::vector<TargetBitrate::BitrateItem>& items) {
    EXPECT_EQ(4, items.size());
    Verify(TargetBitrate::BitrateItem(0, 0, 0x010203), items[0]);
    Verify(TargetBitrate::BitrateItem(0, 1, 0x020304), items[1]);
    Verify(TargetBitrate::BitrateItem(1, 0, 0x030405), items[2]);
    Verify(TargetBitrate::BitrateItem(1, 1, 0x040506), items[3]);
}
    
} // namespace

class T(RtcpPacketExtendedReportsTest) : public ::testing::Test {
protected:
    template <typename T>
    T Rand() {
        return utils::random::generate_random<T>();
    }
};

template<>
Dlrr::SubBlock T(RtcpPacketExtendedReportsTest)::Rand<Dlrr::SubBlock>() {
    uint32_t ssrc = Rand<uint32_t>();
    uint32_t last_rr = Rand<uint32_t>();
    uint32_t delay_since_last_rr = Rand<uint32_t>();
    return Dlrr::SubBlock(ssrc, last_rr, delay_since_last_rr);
}

template<>
NtpTime T(RtcpPacketExtendedReportsTest)::Rand<NtpTime>() {
    uint32_t secs = Rand<uint32_t>();
    uint32_t frac = Rand<uint32_t>();
    return NtpTime(secs, frac);
}

template<>
Rrtr T(RtcpPacketExtendedReportsTest)::Rand<Rrtr>() {
    Rrtr rrtr;
    rrtr.set_ntp(Rand<NtpTime>());
    return rrtr;
}

MY_TEST_F(RtcpPacketExtendedReportsTest, CreateWithoutReportBlocks) {
    ExtendedReports xr;
    xr.set_sender_ssrc(kSenderSsrc);

    auto packet = xr.Build();

    EXPECT_THAT(make_tuple(packet.data(), packet.size()),
                ElementsAreArray(kPacketWithoutBlocks));
}

MY_TEST_F(RtcpPacketExtendedReportsTest, ParseWithoutReportBlocks) {
    ExtendedReports parsed;
    EXPECT_TRUE(test::ParseSinglePacket(kPacketWithoutBlocks, &parsed));
    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    EXPECT_FALSE(parsed.rrtr());
    EXPECT_FALSE(parsed.dlrr());
}

MY_TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithRrtrBlock) {
    const Rrtr kRrtr = Rand<Rrtr>();
    ExtendedReports xr;
    xr.set_sender_ssrc(kSenderSsrc);
    xr.set_rrtr(kRrtr);
    auto packet = xr.Build();

    ExtendedReports mparsed;
    EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
    const ExtendedReports& parsed = mparsed;

    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    EXPECT_EQ(kRrtr, parsed.rrtr());
}

MY_TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithDlrrWithOneSubBlock) {
    const Dlrr::SubBlock kSubBlock = Rand<Dlrr::SubBlock>();
    ExtendedReports xr;
    xr.set_sender_ssrc(kSenderSsrc);
    EXPECT_TRUE(xr.AddDlrrSubBlock(kSubBlock));

    auto packet = xr.Build();

    ExtendedReports mparsed;
    EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
    const ExtendedReports& parsed = mparsed;

    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    EXPECT_THAT(parsed.dlrr().sub_blocks(), ElementsAre(kSubBlock));
}

MY_TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithDlrrWithTwoSubBlocks) {
    const Dlrr::SubBlock kTimeInfo1 = Rand<Dlrr::SubBlock>();
    const Dlrr::SubBlock kTimeInfo2 = Rand<Dlrr::SubBlock>();
    ExtendedReports xr;
    xr.set_sender_ssrc(kSenderSsrc);
    xr.AddDlrrSubBlock(kTimeInfo1);
    xr.AddDlrrSubBlock(kTimeInfo2);

    auto packet = xr.Build();

    ExtendedReports mparsed;
    EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
    const ExtendedReports& parsed = mparsed;

    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    EXPECT_THAT(parsed.dlrr().sub_blocks(), ElementsAre(kTimeInfo1, kTimeInfo2));
}

MY_TEST_F(RtcpPacketExtendedReportsTest, CreateLimitsTheNumberOfDlrrSubBlocks) {
    const Dlrr::SubBlock kTimeInfo = Rand<Dlrr::SubBlock>();
    ExtendedReports xr;

    for (size_t i = 0; i < ExtendedReports::kMaxNumberOfDlrrSubBlocks; ++i) {
        EXPECT_TRUE(xr.AddDlrrSubBlock(kTimeInfo));
    }
    EXPECT_FALSE(xr.AddDlrrSubBlock(kTimeInfo));

    EXPECT_THAT(xr.dlrr().sub_blocks(),
                SizeIs(ExtendedReports::kMaxNumberOfDlrrSubBlocks));
}

MY_TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithMaximumDlrrSubBlocks) {
    const Rrtr kRrtr = Rand<Rrtr>();

    ExtendedReports xr;
    xr.set_sender_ssrc(kSenderSsrc);
    xr.set_rrtr(kRrtr);
    for (size_t i = 0; i < ExtendedReports::kMaxNumberOfDlrrSubBlocks; ++i)
        xr.AddDlrrSubBlock(Rand<Dlrr::SubBlock>());

    auto packet = xr.Build();

    ExtendedReports mparsed;
    EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
    const ExtendedReports& parsed = mparsed;

    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    EXPECT_EQ(kRrtr, parsed.rrtr());
    EXPECT_THAT(parsed.dlrr().sub_blocks(),
                ElementsAreArray(xr.dlrr().sub_blocks()));
}

MY_TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithTargetBitrateBlock) {
    const size_t kXRHeaderSize = 8;  // RTCP header (4) + SSRC (4).
    const size_t kTotalSize = kXRHeaderSize + sizeof(kTargetBitrateBlock);
    uint8_t kRtcpPacket[kTotalSize] = {2 << 6, 207,  0x00, (kTotalSize / 4) - 1,
                                       0x12,   0x34, 0x56, 0x78};  // SSRC.
    memcpy(&kRtcpPacket[kXRHeaderSize], kTargetBitrateBlock, kTargetBitrateBlockSize);
    rtcp::ExtendedReports xr;
    EXPECT_TRUE(ParseSinglePacket(kRtcpPacket, &xr));
    EXPECT_EQ(kSenderSsrc, xr.sender_ssrc());
    const auto& target_bitrate = xr.target_bitrate();
    ASSERT_TRUE(static_cast<bool>(target_bitrate));
    Verify(target_bitrate->GetTargetBitrates());
    
}
    
} // namespace test
} // namespace naivertc
