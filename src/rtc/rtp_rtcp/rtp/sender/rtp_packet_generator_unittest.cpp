#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extension_map.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

#include <vector>

namespace naivertc {
namespace test {
namespace {

const uint32_t kSsrc = 725242;
const uint32_t kRtxSsrc = 12345;

enum : int {
    kTransportSequenceNumberExtensionId = 1,
    kAbsoluteSendTimeExtensionId,
    kAbsolutedCaptureTimeExtensionId,
    kTransmissionOffsetExtensionId,
    kPlayoutDelayLimitsExtensionId,
};
    
} // namespace


class T(RtpPacketGeneratorTest) : public ::testing::Test {
public:
    T(RtpPacketGeneratorTest)() 
        : clock_(1000) {};

    void SetUp() override {
        auto config = GetDefaultConfig();
        packet_generator_ = std::make_unique<RtpPacketGenerator>(config, &header_extension_map_);
    }

    RtpConfiguration GetDefaultConfig() {
        RtpConfiguration config;
        config.audio = false;
        config.clock = &clock_;
        config.local_media_ssrc = kSsrc;
        config.rtx_send_ssrc = kRtxSsrc;
        return config;
    }

protected:
    SimulatedClock clock_;
    rtp::HeaderExtensionMap header_extension_map_;
    std::unique_ptr<RtpPacketGenerator> packet_generator_;
};

MY_TEST_F(RtpPacketGeneratorTest, GeneratePacketSetSsrc) {
    std::vector<uint32_t> csrcs;
    csrcs.push_back(12345);
    packet_generator_->set_csrcs(csrcs);
    auto new_packet = packet_generator_->GeneratePacket();

    EXPECT_EQ(packet_generator_->ssrc(), new_packet.ssrc());
    EXPECT_EQ(csrcs, new_packet.csrcs());
}

MY_TEST_F(RtpPacketGeneratorTest, GeneratePacketReserveExtensions) {
    header_extension_map_.Register<rtp::AbsoluteSendTime>(kAbsoluteSendTimeExtensionId);
    header_extension_map_.Register<rtp::TransmissionTimeOffset>(kTransmissionOffsetExtensionId);
    header_extension_map_.Register<rtp::TransportSequenceNumber>(kTransportSequenceNumberExtensionId);
    header_extension_map_.Register<rtp::AbsoluteCaptureTime>(kAbsolutedCaptureTimeExtensionId);
    header_extension_map_.Register<rtp::PlayoutDelayLimits>(kPlayoutDelayLimitsExtensionId);

    auto new_packet = packet_generator_->GeneratePacket();

    // Preallocate extensions
    EXPECT_TRUE(new_packet.HasExtension<rtp::AbsoluteSendTime>());
    EXPECT_TRUE(new_packet.HasExtension<rtp::TransmissionTimeOffset>());
    EXPECT_TRUE(new_packet.HasExtension<rtp::TransportSequenceNumber>());
    // Do not allocate extensions
    EXPECT_FALSE(new_packet.HasExtension<rtp::AbsoluteCaptureTime>());
    EXPECT_FALSE(new_packet.HasExtension<rtp::PlayoutDelayLimits>());
}

MY_TEST_F(RtpPacketGeneratorTest, BuildRtxPacket) {
    const uint8_t kMediaPayloadType = 98;
    const uint8_t kRtxPayloadType = 99;
    const uint16_t kSeqNum = 123;
    std::vector<uint32_t> csrcs;
    csrcs.push_back(12345);
    packet_generator_->set_csrcs(csrcs);

    auto media_packet = packet_generator_->GeneratePacket();
    media_packet.set_payload_type(kMediaPayloadType);
    media_packet.set_sequence_number(kSeqNum);
    
    // No RTX packet built before setting RTX payload type.
    auto rtx_packet = packet_generator_->BuildRtxPacket(media_packet);
    ASSERT_FALSE(rtx_packet);

    // Associated media payload with RTX payload type.
    packet_generator_->SetRtxPayloadType(kRtxPayloadType, kMediaPayloadType);

    // Build RTX packet from media packet
    rtx_packet = packet_generator_->BuildRtxPacket(media_packet);
    ASSERT_TRUE(rtx_packet);
    EXPECT_EQ(kRtxPayloadType, rtx_packet->payload_type());
    EXPECT_EQ(packet_generator_->rtx_ssrc(), rtx_packet->ssrc());
    EXPECT_EQ(csrcs, rtx_packet->csrcs());
    // The sequence number of RTX packet is not the same with media one.
    EXPECT_NE(kSeqNum, rtx_packet->sequence_number());
}

MY_TEST_F(RtpPacketGeneratorTest, UpdateCsrcsUpdateOverhead) {
    // Base RTP overhead is 12 bytes
    EXPECT_EQ(packet_generator_->MaxMediaPacketHeaderSize(), 12);

    // Add three csrcs add 3 * 4 bytes to the header
    packet_generator_->set_csrcs({12, 22, 33});

    EXPECT_EQ(packet_generator_->MaxMediaPacketHeaderSize(), 24);
}
    
} // namespace test
} // namespace naivertc
