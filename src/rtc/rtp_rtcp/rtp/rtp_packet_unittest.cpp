#include "rtc/rtp_rtcp/rtp/rtp_packet.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace naivertc {
namespace test {

namespace {
constexpr uint8_t kPayload_type = 0x0Fu;
constexpr uint16_t kSequence_num = 0x06FDu;
constexpr uint32_t kTimestamp = 0xF6E8F500u;
constexpr uint32_t kSsrc = 0x00123445u;

constexpr uint8_t kPacket[] = {
    // Header
    0xa0, 0x8f, 0x06, 0xFD, 0xF6, 0xE8, 0xF5, 0x00,
    0x00, 0x12, 0x34, 0x45,
    // Payload
    0xd7, 0xab, 0x2f, 0xd7, 0x37, 0xac, 0x96, 0x71,
    0xbb, 0xda, 0x16, 0xd4, 0xb7, 0x15, 0x49, 0x6f,
    0xf0, 0xb5, 0x1a, 0xae, 0x86, 0x4b, 0xd3, 0x1b,
    0x91, 0x8b, 0x76, 0xd3, 0x01, 0x0f, 0xc9, 0xbf,
    0xdc, 0x2c, 0x9d, 0x59, 0xe3, 0x81, 0xc5, 0x75,
    0x07, 0x0b, 0x58, 0x52, 0x57, 0x65, 0x2d, 0x7a,
    0x4e, 0xb5, 0x50, 0x8d, 0x60, 0xf4, 0xef, 0x6f, 
    0x70, 0xc9, 0x46, 0x4d, 0x7f, 0x62, 0x50, 0xd4,
    0xc2, 0xb2, 0x93, 0xf4, 0x1a, 0x89, 0x99, 0xd4, 
    0x94, 0x49, 0x49, 0x2c, 0xf8, 0x47, 0xea, 0x7e,
    0x57, 0x34, 0xef, 0x64, 0xa5, 0x71, 0xed, 0x7e,
    0xea, 0x4e, 0x96, 0xcd, 0x4f, 0x5e, 0xb0, 0x81,
    // Padding
    0x00, 0x00, 0x00, 0x04
};
constexpr size_t kPacketSize = sizeof(kPacket);

// Payload
constexpr uint8_t kPayload[] = {
    0xd7, 0xab, 0x2f, 0xd7, 0x37, 0xac, 0x96, 0x71,
    0xbb, 0xda, 0x16, 0xd4, 0xb7, 0x15, 0x49, 0x6f,
    0xf0, 0xb5, 0x1a, 0xae, 0x86, 0x4b, 0xd3, 0x1b,
    0x91, 0x8b, 0x76, 0xd3, 0x01, 0x0f, 0xc9, 0xbf,
    0xdc, 0x2c, 0x9d, 0x59, 0xe3, 0x81, 0xc5, 0x75,
    0x07, 0x0b, 0x58, 0x52, 0x57, 0x65, 0x2d, 0x7a,
    0x4e, 0xb5, 0x50, 0x8d, 0x60, 0xf4, 0xef, 0x6f, 
    0x70, 0xc9, 0x46, 0x4d, 0x7f, 0x62, 0x50, 0xd4,
    0xc2, 0xb2, 0x93, 0xf4, 0x1a, 0x89, 0x99, 0xd4, 
    0x94, 0x49, 0x49, 0x2c, 0xf8, 0x47, 0xea, 0x7e,
    0x57, 0x34, 0xef, 0x64, 0xa5, 0x71, 0xed, 0x7e,
    0xea, 0x4e, 0x96, 0xcd, 0x4f, 0x5e, 0xb0, 0x81
};
constexpr size_t kPayloadSize = sizeof(kPayload);
}

TEST(RtpPacketTest, BuildPacket) {
    auto rtp_packet = RtpPacket::Create();
    EXPECT_NE(rtp_packet, nullptr);
    EXPECT_EQ(rtp_packet->capacity(), 1500);
    EXPECT_EQ(rtp_packet->payload_size(), 0);
    EXPECT_EQ(rtp_packet->has_padding(), false);
    EXPECT_EQ(rtp_packet->padding_size(), 0);

    rtp_packet->set_marker(true);
    rtp_packet->set_payload_type(kPayload_type);
    rtp_packet->set_sequence_number(kSequence_num);
    rtp_packet->set_timestamp(kTimestamp);
    rtp_packet->set_ssrc(kSsrc);
    rtp_packet->set_payload(kPayload, kPayloadSize);
    EXPECT_TRUE(rtp_packet->SetPadding(4));

    EXPECT_EQ(rtp_packet->marker(), true);
    EXPECT_EQ(rtp_packet->has_padding(), true);
    EXPECT_EQ(rtp_packet->padding_size(), 4);
    EXPECT_EQ(rtp_packet->payload_type(), kPayload_type);
    EXPECT_EQ(rtp_packet->sequence_number(), kSequence_num);
    EXPECT_EQ(rtp_packet->timestamp(), kTimestamp);
    EXPECT_EQ(rtp_packet->ssrc(), kSsrc);
    EXPECT_EQ(rtp_packet->header_size(), 12);
    EXPECT_EQ(rtp_packet->payload_size(), kPayloadSize);

    EXPECT_THAT(std::make_tuple(rtp_packet->payload_data(), rtp_packet->payload_size()), testing::ElementsAreArray(kPayload, kPayloadSize));
}

TEST(RtpPacketTest, Parse) {
    auto rtp_packet = RtpPacket::Create();
    EXPECT_NE(rtp_packet, nullptr);
    EXPECT_EQ(rtp_packet->capacity(), 1500);

    EXPECT_TRUE(rtp_packet->Parse(kPacket, kPacketSize));

    EXPECT_EQ(rtp_packet->marker(), true);
    EXPECT_EQ(rtp_packet->has_padding(), true);
    EXPECT_EQ(rtp_packet->padding_size(), 4);
    EXPECT_EQ(rtp_packet->payload_type(), kPayload_type);
    EXPECT_EQ(rtp_packet->sequence_number(), kSequence_num);
    EXPECT_EQ(rtp_packet->timestamp(), kTimestamp);
    EXPECT_EQ(rtp_packet->ssrc(), kSsrc);
    EXPECT_EQ(rtp_packet->header_size(), 12);
    EXPECT_EQ(rtp_packet->payload_size(), kPayloadSize);
}

TEST(RtpPacketTest, TwiceSet) {
    auto rtp_packet = RtpPacket::Create();

    rtp_packet->set_marker(true);
    rtp_packet->set_marker(true);

    EXPECT_TRUE(rtp_packet->marker());
    EXPECT_EQ(0x80, rtp_packet->data()[1] & 0x80);

    rtp_packet->set_marker(false);
    EXPECT_FALSE(rtp_packet->marker());
    EXPECT_EQ(0x00, rtp_packet->data()[1] & 0x80);

    rtp_packet->set_has_padding(true);
    rtp_packet->set_has_padding(true);
    EXPECT_TRUE(rtp_packet->has_padding());
    EXPECT_EQ(0x20, rtp_packet->data()[0] & 0x20);

    rtp_packet->set_has_padding(false);
    EXPECT_FALSE(rtp_packet->has_padding());
    EXPECT_EQ(0x00, rtp_packet->data()[0] & 0x00);
}
    
} // namespace test
} // namespace naivertc 