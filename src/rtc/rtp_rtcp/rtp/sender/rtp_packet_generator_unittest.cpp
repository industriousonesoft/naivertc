#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.hpp"

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
        packet_generator_ = std::make_unique<RtpPacketGenerator>(GetDefaultConfig());
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
    packet_generator_->Register(rtp::AbsoluteSendTime::kUri, kAbsoluteSendTimeExtensionId);
    packet_generator_->Register(rtp::TransmissionTimeOffset::kUri, kTransmissionOffsetExtensionId);
    packet_generator_->Register(rtp::TransportSequenceNumber::kUri, kTransportSequenceNumberExtensionId);

    packet_generator_->Register(rtp::AbsoluteCaptureTime::kUri, kAbsolutedCaptureTimeExtensionId);
    packet_generator_->Register(rtp::PlayoutDelayLimits::kUri, kPlayoutDelayLimitsExtensionId);

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
    std::vector<uint32_t> csrcs;
    csrcs.push_back(12345);
    packet_generator_->set_csrcs(csrcs);

    auto media_packet = packet_generator_->GeneratePacket();
    media_packet.set_payload_type(kMediaPayloadType);
    
    // No RTX packet built before setting RTX payload type.
    auto rtx_packet = packet_generator_->BuildRtxPacket(media_packet);
    ASSERT_FALSE(rtx_packet);

    packet_generator_->SetRtxPayloadType(kRtxPayloadType, kMediaPayloadType);
    rtx_packet = packet_generator_->BuildRtxPacket(media_packet);
    ASSERT_TRUE(rtx_packet);
    EXPECT_EQ(kRtxPayloadType, rtx_packet->payload_type());
    EXPECT_EQ(packet_generator_->rtx_ssrc(), rtx_packet->ssrc());
    EXPECT_EQ(csrcs, rtx_packet->csrcs());
}
    
} // namespace test
} // namespace naivertc
