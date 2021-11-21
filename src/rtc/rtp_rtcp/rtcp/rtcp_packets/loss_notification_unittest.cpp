#include "rtc/rtp_rtcp/rtcp/rtcp_packets/loss_notification.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {

MY_TEST(RtcpPacketLossNotificationTest, SetWithIllegalValuesFails) {
    constexpr uint16_t kLastDecoded = 0x3c7b;
    constexpr uint16_t kLastReceived = kLastDecoded + 0x7fff + 1;
    constexpr bool kDecodabilityFlag = true;
    LossNotification loss_notification;
    EXPECT_FALSE(loss_notification.Set(kLastDecoded, kLastReceived, kDecodabilityFlag));
}

MY_TEST(RtcpPacketLossNotificationTest, SetWithLegalValuesSucceeds) {
    constexpr uint16_t kLastDecoded = 0x3c7b;
    constexpr uint16_t kLastReceived = kLastDecoded + 0x7fff;
    constexpr bool kDecodabilityFlag = true;
    LossNotification loss_notification;
    EXPECT_TRUE(loss_notification.Set(kLastDecoded, kLastReceived, kDecodabilityFlag));
}

MY_TEST(RtcpPacketLossNotificationTest, CreateProducesExpectedWireFormat) {
    // Note that (0x6542 >> 1) is used just to make the pattern in kPacket
    // more apparent; there's nothing truly special about the value,
    // it's only an implementation detail that last-received is represented
    // as a delta from last-decoded, and that this delta is shifted before
    // it's put on the wire.
    constexpr uint16_t kLastDecoded = 0x3c7b;
    constexpr uint16_t kLastReceived = kLastDecoded + (0x6542 >> 1);
    constexpr bool kDecodabilityFlag = true;

    const uint8_t kPacket[] = {0x8f, 206,  0x00, 0x04, 0x12, 0x34, 0x56, 0x78,  //
                               0xab, 0xcd, 0xef, 0x01, 'L',  'N',  'T',  'F',   //
                               0x3c, 0x7b, 0x65, 0x43};

    LossNotification loss_notification;
    loss_notification.set_sender_ssrc(0x12345678);
    loss_notification.set_media_ssrc(0xabcdef01);
    ASSERT_TRUE(loss_notification.Set(kLastDecoded, kLastReceived, kDecodabilityFlag));

    BinaryBuffer packet = loss_notification.Build();

    EXPECT_THAT(packet, testing::ElementsAreArray(kPacket));
}

MY_TEST(RtcpPacketLossNotificationTest, ParseLegalLossNotificationMessagesCorrectly) {
     // Note that (0x6542 >> 1) is used just to make the pattern in kPacket
    // more apparent; there's nothing truly special about the value,
    // it's only an implementation detail that last-received is represented
    // as a delta from last-decoded, and that this delta is shifted before
    // it's put on the wire.
    constexpr uint16_t kLastDecoded = 0x3c7b;
    constexpr uint16_t kLastReceived = kLastDecoded + (0x6542 >> 1);
    constexpr bool kDecodabilityFlag = true;

    uint8_t packet[] = {0x8f, 206,  0x00, 0x04, 0x12, 0x34, 0x56, 0x78,  //
                        0xab, 0xcd, 0xef, 0x01, 'L',  'N',  'T',  'F',   //
                        0x3c, 0x7b, 0x65, 0x43};
    size_t packet_size = 20;
  
    // First, prove that the failure we're expecting to see happens because of
    // the length, by showing that before the modification to the length,
    // the packet was correctly parsed.
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(packet, packet_size));

    LossNotification loss_notification;
    EXPECT_TRUE(loss_notification.Parse(common_header));

    EXPECT_EQ(loss_notification.sender_ssrc(), 0x12345678u);
    EXPECT_EQ(loss_notification.media_ssrc(), 0xabcdef01u);
    EXPECT_EQ(loss_notification.last_decoded(), kLastDecoded);
    EXPECT_EQ(loss_notification.last_received(), kLastReceived);
    EXPECT_EQ(loss_notification.decodability_flag(), kDecodabilityFlag);
}
    
} // namespace test
} // namespace naivertc
