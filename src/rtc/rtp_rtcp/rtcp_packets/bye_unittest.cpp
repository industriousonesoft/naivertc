#include "rtc/rtp_rtcp/rtcp_packets/bye.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/common_header.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {
namespace {
constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint32_t kCsrc1 = 0x22232425;
constexpr uint32_t kCsrc2 = 0x33343536;
}  // namespace


TEST(RtcpPacketByeTest, CreateAndParseWithoutReason) {
    Bye bye;
    bye.set_sender_ssrc(kSenderSsrc);

    BinaryBuffer raw = bye.Build();
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(raw.data(), raw.size()));
    Bye parsed_bye;
    EXPECT_TRUE(parsed_bye.Parse(common_header));

    EXPECT_EQ(kSenderSsrc, parsed_bye.sender_ssrc());
    EXPECT_TRUE(parsed_bye.csrcs().empty());
    EXPECT_TRUE(parsed_bye.reason().empty());
}

TEST(RtcpPacketByeTest, CreateAndParseWithCsrcs) {
    Bye bye;
    bye.set_sender_ssrc(kSenderSsrc);
    EXPECT_TRUE(bye.set_csrcs({kCsrc1, kCsrc2}));
    EXPECT_TRUE(bye.reason().empty());

    BinaryBuffer raw = bye.Build();
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(raw.data(), raw.size()));
    Bye parsed_bye;
    EXPECT_TRUE(parsed_bye.Parse(common_header));

    EXPECT_EQ(kSenderSsrc, parsed_bye.sender_ssrc());
    EXPECT_THAT(parsed_bye.csrcs(), testing::ElementsAre(kCsrc1, kCsrc2));
    EXPECT_TRUE(parsed_bye.reason().empty());
}

TEST(RtcpPacketByeTest, CreateAndParseWithCsrcsAndAReason) {
    Bye bye;
    const std::string kReason = "Some Reason";

    bye.set_sender_ssrc(kSenderSsrc);
    EXPECT_TRUE(bye.set_csrcs({kCsrc1, kCsrc2}));
    bye.set_reason(kReason);

    BinaryBuffer raw = bye.Build();
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(raw.data(), raw.size()));
    Bye parsed_bye;
    EXPECT_TRUE(parsed_bye.Parse(common_header));

    EXPECT_EQ(kSenderSsrc, parsed_bye.sender_ssrc());
    EXPECT_THAT(parsed_bye.csrcs(), testing::ElementsAre(kCsrc1, kCsrc2));
    EXPECT_EQ(kReason, parsed_bye.reason());
}

TEST(RtcpPacketByeTest, CreateWithTooManyCsrcs) {
    Bye bye;
    bye.set_sender_ssrc(kSenderSsrc);
    const int kMaxCsrcs = (1 << 5) - 2;  // 5 bit len, first item is sender SSRC.
    EXPECT_TRUE(bye.set_csrcs(std::vector<uint32_t>(kMaxCsrcs, kCsrc1)));
    EXPECT_FALSE(bye.set_csrcs(std::vector<uint32_t>(kMaxCsrcs + 1, kCsrc1)));
}

TEST(RtcpPacketByeTest, CreateAndParseWithAReason) {
    Bye bye;
    const std::string kReason = "Some Random Reason";

    bye.set_sender_ssrc(kSenderSsrc);
    bye.set_reason(kReason);

    BinaryBuffer raw = bye.Build();
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(raw.data(), raw.size()));
    Bye parsed_bye;
    EXPECT_TRUE(parsed_bye.Parse(common_header));

    EXPECT_EQ(kSenderSsrc, parsed_bye.sender_ssrc());
    EXPECT_TRUE(parsed_bye.csrcs().empty());
    EXPECT_EQ(kReason, parsed_bye.reason());
}


TEST(RtcpPacketByeTest, CreateAndParseWithReasons) {
    // Test that packet creation/parsing behave with reasons of different length
    // both when it require padding and when it does not.
    for (size_t reminder = 0; reminder < 4; ++reminder) {
        const std::string kReason(4 + reminder, 'a' + reminder);
        Bye bye;
        bye.set_sender_ssrc(kSenderSsrc);
        bye.set_reason(kReason);

        BinaryBuffer raw = bye.Build();
        CommonHeader common_header;
        EXPECT_TRUE(common_header.Parse(raw.data(), raw.size()));
        Bye parsed_bye;
        EXPECT_TRUE(parsed_bye.Parse(common_header));

        EXPECT_EQ(kReason, parsed_bye.reason());
    }
}

TEST(RtcpPacketByeTest, ParseEmptyPacket) {
    uint8_t kEmptyPacket[] = {0x80, Bye::kPacketType, 0, 0};
    
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kEmptyPacket, sizeof(kEmptyPacket)));
    Bye parsed_bye;
    EXPECT_TRUE(parsed_bye.Parse(common_header));

    EXPECT_EQ(0u, parsed_bye.sender_ssrc());
    EXPECT_TRUE(parsed_bye.csrcs().empty());
    EXPECT_TRUE(parsed_bye.reason().empty());
}

TEST(RtcpPacketByeTest, ParseFailOnInvalidSrcCount) {
    Bye bye;
    bye.set_sender_ssrc(kSenderSsrc);

    BinaryBuffer raw = bye.Build();
    raw[0]++;  // Damage the packet: increase ssrc count by one.
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(raw.data(), raw.size()));
    Bye parsed_bye;
    EXPECT_FALSE(parsed_bye.Parse(common_header));

}

TEST(RtcpPacketByeTest, ParseFailOnInvalidReasonLength) {
    Bye bye;
    bye.set_sender_ssrc(kSenderSsrc);
    bye.set_reason("18 characters long");

    BinaryBuffer raw = bye.Build();
    // Damage the packet: decrease payload size by 4 bytes
    raw[3]--;
    raw.resize(raw.size() - 4);

    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(raw.data(), raw.size()));
    Bye parsed_bye;
    EXPECT_FALSE(parsed_bye.Parse(common_header));
}
    
} // namespace test    
} // namespace naivertc

