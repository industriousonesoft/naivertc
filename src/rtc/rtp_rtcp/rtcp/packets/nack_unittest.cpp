#include "rtc/rtp_rtcp/rtcp/packets/nack.hpp"
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

constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint32_t kRemoteSsrc = 0x23456789;

constexpr uint16_t kList[] = {0, 1, 3, 8, 16};
constexpr size_t kListSize = sizeof(kList) / sizeof(kList[0]);
constexpr uint8_t kVersionBits = 2 << 6;
// clang-format off
constexpr uint8_t kPacket[] = {
    kVersionBits | Nack::kFeedbackMessageType, Nack::kPacketType, 0, 3,
    0x12, 0x34, 0x56, 0x78,
    0x23, 0x45, 0x67, 0x89,
    0x00, 0x00, 0x80, 0x85};
constexpr size_t kPacketSize = sizeof(kPacket);

} // namespace

MY_TEST(RtcpNackTest, Parse) {
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kPacket, kPacketSize));
    EXPECT_EQ(Nack::kFeedbackMessageType, common_header.feedback_message_type());
    EXPECT_EQ(Nack::kPacketType, common_header.type());
    EXPECT_EQ(kPacketSize, common_header.packet_size());

    Nack nack;
    EXPECT_TRUE(nack.Parse(common_header));
    EXPECT_EQ(kSenderSsrc, nack.sender_ssrc());
    EXPECT_EQ(kRemoteSsrc, nack.media_ssrc());
    EXPECT_THAT(nack.packet_ids(), testing::ElementsAreArray(kList));
}

MY_TEST(RtcpNackTest, Create) {
    Nack nack;
    nack.set_sender_ssrc(kSenderSsrc);
    nack.set_media_ssrc(kRemoteSsrc);
    nack.set_packet_ids(kList, kListSize);

    EXPECT_EQ(5u, nack.packet_ids().size());
    EXPECT_EQ(16u, nack.PacketSize());

    auto packet = nack.Build();
    EXPECT_THAT(ArrayView<const uint8_t>(packet), testing::ElementsAreArray(kPacket));
}

MY_TEST(RtcpNackTest, CreateFragment) {
    Nack nack;
    const uint16_t kList[] = {1, 100, 200, 300, 400};
    const size_t kListSize = sizeof(kList) / sizeof(kList[0]);
    nack.set_sender_ssrc(kSenderSsrc);
    nack.set_media_ssrc(kRemoteSsrc);
    nack.set_packet_ids(kList, kListSize);

    // Rtcp common header + Payload-specific feedback common fields + 3 nack items
    const size_t kBufferSize = 4 + 8 + (3 * 4);
    testing::MockFunction<void(CopyOnWriteBuffer)> callback;
    EXPECT_CALL(callback, Call(testing::_))
        .WillOnce(testing::Invoke([&](CopyOnWriteBuffer packet){
            CommonHeader common_header;
            EXPECT_TRUE(common_header.Parse(packet.data(), packet.size()));
            Nack nack;
            EXPECT_TRUE(nack.Parse(common_header));
            EXPECT_EQ(kSenderSsrc, nack.sender_ssrc());
            EXPECT_EQ(kRemoteSsrc, nack.media_ssrc());
            EXPECT_THAT(nack.packet_ids(), testing::ElementsAre(1, 100, 200));
        }))
        .WillOnce(testing::Invoke([&](CopyOnWriteBuffer packet){
            CommonHeader common_header;
            EXPECT_TRUE(common_header.Parse(packet.data(), packet.size()));
            Nack nack;
            EXPECT_TRUE(nack.Parse(common_header));
            EXPECT_EQ(kSenderSsrc, nack.sender_ssrc());
            EXPECT_EQ(kRemoteSsrc, nack.media_ssrc());
            EXPECT_THAT(nack.packet_ids(), testing::ElementsAre(300, 400));
        }));

    EXPECT_TRUE(nack.Build(kBufferSize, callback.AsStdFunction()));
}

}
} // namespace naivertc
