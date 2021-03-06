#include "rtc/rtp_rtcp/rtcp/packets/pli.hpp"
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
// Manually created Pli packet matching constants above.
const uint8_t kPacket[] = {0x81, 206,  0x00, 0x02, 
                           0x12, 0x34, 0x56, 0x78, 
                           0x23, 0x45, 0x67, 0x89};
}  // namespace

MY_TEST(RtcpPliTest, Parse) {
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

MY_TEST(RtcpPliTest, Create) {
    Pli pli;
    pli.set_sender_ssrc(kSenderSsrc);
    pli.set_media_ssrc(kRemoteSsrc);

    auto packet = pli.Build();

    EXPECT_THAT(ArrayView<const uint8_t>(packet), testing::ElementsAreArray(kPacket));
}

} // namespace test
} // namespace naivert 
