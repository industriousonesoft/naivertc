#include "rtc/rtp_rtcp/rtcp/packets/sdes.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/common_header.hpp"

#include <gtest/gtest.h>
#include <sstream>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {

namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint8_t kPadding = 0;
const uint8_t kTerminatorTag = 0;
const uint8_t kCnameTag = 1;
const uint8_t kNameTag = 2;
const uint8_t kEmailTag = 3;
} // namespace

MY_TEST(RtcpSdesTest, CreateAndParseWithoutChunks) {
    Sdes sdes;
    BinaryBuffer packet = sdes.Build();
    
    Sdes parsed;
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(packet.data(), packet.size()));
    EXPECT_TRUE(parsed.Parse(common_header));

    EXPECT_EQ(0u, parsed.chunks().size());
}

MY_TEST(RtcpSdesTest, CreateAndParseWithOneChunk) {
    const std::string cname = "alice@host";

    Sdes sdes;
    EXPECT_TRUE(sdes.AddCName(kSenderSsrc, cname));
    EXPECT_EQ(1u, sdes.chunks().size());

    BinaryBuffer packet = sdes.Build();
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(packet.data(), packet.size()));
    EXPECT_EQ(Sdes::kPacketType, common_header.type());
    EXPECT_EQ(0, common_header.payload_size() % 4);
    EXPECT_EQ(1u, common_header.count());

    Sdes parsed;
    EXPECT_TRUE(parsed.Parse(common_header));
    EXPECT_EQ(1u, parsed.chunks().size());
    EXPECT_EQ(kSenderSsrc, parsed.chunks()[0].ssrc);
    EXPECT_EQ(cname, parsed.chunks()[0].cname);

}

MY_TEST(RtcpSdesTest, CreateAndParseMultipleChunks) {
    Sdes sdes;
    EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 0, "a"));
    EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 1, "ab"));
    EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 2, "abc"));
    EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 3, "abcd"));
    EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 4, "abcde"));
    EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 5, "abcdef"));
    EXPECT_EQ(6u, sdes.chunks().size());

    BinaryBuffer packet = sdes.Build();
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(packet.data(), packet.size()));

    Sdes parsed;
    EXPECT_TRUE(parsed.Parse(common_header));
    EXPECT_EQ(6u, parsed.chunks().size());
    EXPECT_EQ(kSenderSsrc + 5, parsed.chunks()[5].ssrc);
    EXPECT_EQ("abcdef", parsed.chunks()[5].cname);
    
}

MY_TEST(RtcpSdesTest, CreateWithToManyChunks) {
    const size_t kMaxChunks = (1 << 5) - 1; // 0xFFu
    Sdes sdes;
    
    for (size_t i = 0; i < kMaxChunks; i++) {
        std::ostringstream oss;
        oss << "cname#" << i;
        EXPECT_TRUE(sdes.AddCName(kSenderSsrc + i, oss.str()));
    }
    EXPECT_FALSE(sdes.AddCName(kSenderSsrc + kMaxChunks, "foo"));
}

MY_TEST(RtcpSdesTest, ParseSkipNonCNameField) {
    const uint8_t kName[] = "abc";
    const uint8_t kCName[] = "de";
    const uint8_t kValidPacket[] = {
        0x81, 202, 0x00, 0x04, 0x12, 0x34, 0x56, 0x78, 
        kNameTag, 3, kName[0], kName[1], kName[2], // non-cname
        kCnameTag, 2, kCName[0], kCName[1], // cname
        kTerminatorTag, kPadding, kPadding
    };

    ASSERT_EQ(0u, sizeof(kValidPacket) % 4);
    ASSERT_EQ(kValidPacket[3] + 1u /* payload size + common header size in 32-bit word */, sizeof(kValidPacket) / 4);

    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kValidPacket, sizeof(kValidPacket)));

    Sdes parsed;
    EXPECT_TRUE(parsed.Parse(common_header));
    EXPECT_EQ(1u, parsed.chunks().size());
    EXPECT_EQ(kSenderSsrc, parsed.chunks()[0].ssrc);
    EXPECT_EQ("de", parsed.chunks()[0].cname);
}

} // namespace test
} // namespace naivertc
