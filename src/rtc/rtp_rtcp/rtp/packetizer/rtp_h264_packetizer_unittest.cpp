#include "rtc/rtp_rtcp/rtp/packetizer/rtp_h264_packetizer.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace naivertc;

namespace naivertc {
namespace test {

std::vector<RtpPacketToSend> FetchAllPackets(RtpH264Packetizer* packetizer) {
    std::vector<RtpPacketToSend> result;
    size_t num_packets = packetizer->NumberOfPackets();
    result.reserve(num_packets);
    RtpPacketToSend packet(nullptr);
    while (packetizer->NextPacket(&packet)) {
        result.push_back(packet);
    }
    EXPECT_EQ(result.size(), num_packets);
    return result;
}

class RtpH264PacketizerTest : public ::testing::TestWithParam<h264::PacketizationMode> {};

INSTANTIATE_TEST_SUITE_P(
    PacketizationMode,
    RtpH264PacketizerTest,
    ::testing::Values(h264::PacketizationMode::SINGLE_NAL_UNIT,
                      h264::PacketizationMode::NON_INTERLEAVED));

TEST_P(RtpH264PacketizerTest, SingleNalu) {
    const uint8_t frame[] = {0, 0, 1, uint8_t(h264::NaluType::IDR), 0xFF};

    RtpPacketizer::PayloadSizeLimits limits;
    RtpH264Packetizer packetizer(ArrayView(frame, sizeof(frame)), limits, GetParam());
    EXPECT_EQ(packetizer.NumberOfPackets(), 1u);

    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    EXPECT_EQ(packets.size(), 1u);
    EXPECT_THAT(packets[0].PayloadBuffer(), ::testing::ElementsAre(uint8_t(h264::NaluType::IDR), 0xFF));
}

} // namespace test
} // namespace naivertc
