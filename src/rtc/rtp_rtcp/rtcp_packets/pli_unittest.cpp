#include "rtc/rtp_rtcp/rtcp_packets/pli.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/common_header.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {

namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
// Manually created Pli packet matching constants above.
const uint8_t kPacket[] = {0x81, 206,  0x00, 0x02, 
                           0x12, 0x34, 0x56, 0x78, 
                           0x23, 0x45, 0x67, 0x89};
}  // namespace

TEST(RtcpPliTest, Parse) {
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kPacket, sizeof(kPacket)));
    EXPECT_EQ(Pli::kPacketType, common_header.type());
    EXPECT_EQ(Pli::kFeedbackMessageType, common_header.feedback_message_type());
    EXPECT_EQ(sizeof(kPacket) - 4, common_header.payload_size());

    Pli pli;
    pli.Parse(common_header);
    EXPECT_EQ(kSenderSsrc, pli.sender_ssrc());
    EXPECT_EQ(kRemoteSsrc, pli.media_ssrc());
    EXPECT_EQ(sizeof(kPacket), pli.PacketSize());
}

TEST(RtcpPliTest, Create) {
    Pli pli;
    pli.set_sender_ssrc(kSenderSsrc);
    pli.set_media_ssrc(kRemoteSsrc);

    BinaryBuffer raw = pli.Build();

    EXPECT_THAT(std::make_tuple(raw.data(), raw.size()), testing::ElementsAreArray(kPacket));
}

} // namespace test
} // namespace naivert 
