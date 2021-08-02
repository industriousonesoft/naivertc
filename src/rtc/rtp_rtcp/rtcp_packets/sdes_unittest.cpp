#include "rtc/rtp_rtcp/rtcp_packets/sdes.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/common_header.hpp"

#include <gtest/gtest.h>

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

TEST(RtcpSdesTest, CreateAndParseWithoutChunks) {
    Sdes sdes;
    BinaryBuffer packet = sdes.Build();
    
    Sdes parsed;
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(packet.data(), packet.size()));
    EXPECT_TRUE(parsed.Parse(common_header));

    EXPECT_EQ(0u, parsed.chunks().size());
}

TEST(RtcpSdesTest, CreateAndParseWithOneChunk) {
    const std::string cname = "alice@host";

    Sdes sdes;
    EXPECT_TRUE(sdes.AddCName(kSenderSsrc, cname));
    BinaryBuffer packet = sdes.Build();

    Sdes parsed;
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(packet.data(), packet.size()));
    EXPECT_TRUE(parsed.Parse(common_header));

    EXPECT_EQ(1u, common_header.count());

    EXPECT_EQ(1u, parsed.chunks().size());
    EXPECT_EQ(kSenderSsrc, parsed.chunks()[0].ssrc);
    EXPECT_EQ(cname, parsed.chunks()[0].cname);

}

} // namespace test
} // namespace naivertc
