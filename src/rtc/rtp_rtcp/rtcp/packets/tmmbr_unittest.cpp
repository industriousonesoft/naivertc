#include "rtc/rtp_rtcp/rtcp/packets/tmmbr.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/common_header.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {
namespace {
constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint32_t kRemoteSsrc = 0x23456789;
constexpr uint32_t kBitrateBps = 312000;
constexpr uint16_t kOverhead = 0x1fe;
constexpr uint8_t kPacket[] = {0x83, 205,  0x00, 0x04, 0x12, 0x34, 0x56,
                           0x78, 0x00, 0x00, 0x00, 0x00, 0x23, 0x45,
                           0x67, 0x89, 0x0a, 0x61, 0x61, 0xfe};
constexpr size_t kPacketSize = sizeof(kPacket);
}  // namespace

MY_TEST(RtcpPacketTmmbrTest, Create) {
    Tmmbr tmmbr;
    tmmbr.set_sender_ssrc(kSenderSsrc);
    tmmbr.AddTmmbr(TmmbItem(kRemoteSsrc, kBitrateBps, kOverhead));

    BinaryBuffer packet = tmmbr.Build();
    EXPECT_THAT(packet, testing::ElementsAreArray(kPacket));
}

MY_TEST(RtcpPacketTmmbrTest, Parse) {
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kPacket, kPacketSize));
    
    Tmmbr parsed;
    EXPECT_TRUE(parsed.Parse(common_header));
    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    ASSERT_EQ(1u, parsed.requests().size());
    EXPECT_EQ(kRemoteSsrc, parsed.requests().front().ssrc());
    EXPECT_EQ(kBitrateBps, parsed.requests().front().bitrate_bps());
    EXPECT_EQ(kOverhead, parsed.requests().front().packet_overhead());
}

MY_TEST(RtcpPacketTmmbrTest, CreateAndParseWithTwoEntries) {
    Tmmbr tmmbr;
    tmmbr.set_sender_ssrc(kSenderSsrc);
    tmmbr.AddTmmbr(TmmbItem(kRemoteSsrc, kBitrateBps, kOverhead));
    tmmbr.AddTmmbr(TmmbItem(kRemoteSsrc + 1, 4 * kBitrateBps, kOverhead + 1));

    BinaryBuffer packet = tmmbr.Build();

    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(packet.data(), packet.size()));
    
    Tmmbr parsed;
    EXPECT_TRUE(parsed.Parse(common_header));

    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    EXPECT_EQ(2u, parsed.requests().size());
    EXPECT_EQ(kRemoteSsrc, parsed.requests()[0].ssrc());
    EXPECT_EQ(kRemoteSsrc + 1, parsed.requests()[1].ssrc());
}

MY_TEST(RtcpPacketTmmbrTest, ParseFailsWithoutItems) {
    const uint8_t kZeroItemsPacket[] = {0x83, 205,  0x00, 0x02, 0x12, 0x34,
                                        0x56, 0x78, 0x00, 0x00, 0x00, 0x00};

    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kZeroItemsPacket, sizeof(kZeroItemsPacket)));
    
    Tmmbr parsed;
    EXPECT_FALSE(parsed.Parse(common_header));
}

MY_TEST(RtcpPacketTmmbrTest, ParseFailsOnUnAlignedPacket) {
    const uint8_t kUnalignedPacket[] = {
        0x83, 205,  0x00, 0x05, 0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00,
        0x23, 0x45, 0x67, 0x89, 0x0a, 0x61, 0x61, 0xfe, 0x34, 0x56, 0x78, 0x9a};

    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kUnalignedPacket, sizeof(kUnalignedPacket)));
    
    Tmmbr parsed;
    EXPECT_FALSE(parsed.Parse(common_header));
}
    
} // namespace test
} // namespace naivertc