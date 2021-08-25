#include "rtc/rtp_rtcp/rtp/packetizer/rtp_h264_packetizer.hpp"
#include "common/array_view.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace naivertc;

namespace naivertc {
namespace test {

static constexpr size_t kMaxPayloadSize = 1200;
static constexpr size_t kNalHeaderSize = 1;
static constexpr size_t kFuAHeaderSize = 2;
static constexpr size_t kLengthFieldLength = 2;

BinaryBuffer GenerateNalUnit(size_t size, h264::NaluType type = h264::NaluType::IDR) {
    BinaryBuffer nalu_buffer(size);
    if (size == 0) {
        return nalu_buffer;
    }
    nalu_buffer[0] = uint8_t(type);
    for (size_t i = 0; i < size; ++i) {
        nalu_buffer[i] = static_cast<uint8_t>(i);
    }
    // Last byte shouldn't be 0, or it may be counted as part of next 4-byte start sequence.
    nalu_buffer[size - 1] = 0x10;
    return nalu_buffer;
}

BinaryBuffer CreateFrame(ArrayView<const BinaryBuffer> nalus) {
    static constexpr size_t kStartCodeSize = 3;
    size_t frame_size = 0;
    for (const BinaryBuffer& nalu : nalus) {
        frame_size += (kStartCodeSize + nalu.size());
    }
    BinaryBuffer frame(frame_size);
    size_t offset = 0;
    for (const BinaryBuffer& nalu : nalus) {
        // Insert nalu start code
        frame[offset] = 0;
        frame[offset + 1] = 0;
        frame[offset + 2] = 1;
        // Copy the nalu unit.
        memcpy(frame.data() + offset + kStartCodeSize, nalu.data(), nalu.size());
        offset += (kStartCodeSize + nalu.size());
    }
    return frame;
}

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
    EXPECT_THAT(packets[0].payload(), ::testing::ElementsAre(uint8_t(h264::NaluType::IDR), 0xFF));
}

TEST_P(RtpH264PacketizerTest, SingleNaluTwoPackets) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = kMaxPayloadSize;

    BinaryBuffer nalus[] = {GenerateNalUnit(kMaxPayloadSize),
                                         GenerateNalUnit(100)};
    EXPECT_EQ(nalus[0].size(), kMaxPayloadSize);
    EXPECT_EQ(nalus[1].size(), 100);
    BinaryBuffer frame = CreateFrame(nalus);

    RtpH264Packetizer packetizer(frame, limits, GetParam());
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    ASSERT_THAT(packets, testing::SizeIs(2));
    EXPECT_THAT(packets[0].payload(), testing::ElementsAreArray(nalus[0]));
    EXPECT_THAT(packets[1].payload(), testing::ElementsAreArray(nalus[1]));
}

TEST_P(RtpH264PacketizerTest, SingleNaluFirstPacketReductionAppliesOnlyToFirstFragment) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 200;
    limits.first_packet_reduction_size = 5;
    BinaryBuffer nalus[] = {GenerateNalUnit(/*size=*/195),
                            GenerateNalUnit(/*size=*/200),
                            GenerateNalUnit(/*size=*/200)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpH264Packetizer packetizer(frame, limits, GetParam());
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    ASSERT_THAT(packets, ::testing::SizeIs(3));
    EXPECT_THAT(packets[0].payload(), ::testing::ElementsAreArray(nalus[0]));
    EXPECT_THAT(packets[1].payload(), ::testing::ElementsAreArray(nalus[1]));
    EXPECT_THAT(packets[2].payload(), ::testing::ElementsAreArray(nalus[2]));
}

TEST_P(RtpH264PacketizerTest, SingleNaluLastPacketReductionAppliesOnlyToLastFragment) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 200;
    limits.last_packet_reduction_size = 5;
    BinaryBuffer nalus[] = {GenerateNalUnit(/*size=*/200),
                           GenerateNalUnit(/*size=*/200),
                           GenerateNalUnit(/*size=*/195)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpH264Packetizer packetizer(frame, limits, GetParam());
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    ASSERT_THAT(packets, ::testing::SizeIs(3));
    EXPECT_THAT(packets[0].payload(), ::testing::ElementsAreArray(nalus[0]));
    EXPECT_THAT(packets[1].payload(), ::testing::ElementsAreArray(nalus[1]));
    EXPECT_THAT(packets[2].payload(), ::testing::ElementsAreArray(nalus[2]));
}

TEST_P(RtpH264PacketizerTest, SingleNaluFirstAndLastPacketReductionSumsForSinglePacket) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 200;
    limits.first_packet_reduction_size = 20;
    limits.last_packet_reduction_size = 30;
    BinaryBuffer nalus[] = {GenerateNalUnit(/*size=*/150)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpH264Packetizer packetizer(frame, limits, GetParam());
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    EXPECT_THAT(packets, ::testing::SizeIs(1));
}

// Aggregation tests.
TEST(RtpH264PacketizerTest, StapA) {
    BinaryBuffer nalus[] = {GenerateNalUnit(/*size=*/2),
                            GenerateNalUnit(/*size=*/2),
                            GenerateNalUnit(/*size=*/0x123)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpPacketizer::PayloadSizeLimits limits;
    RtpH264Packetizer packetizer(frame, limits, h264::PacketizationMode::NON_INTERLEAVED);
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    ASSERT_THAT(packets, ::testing::SizeIs(1));
    auto payload = packets[0].payload();
    EXPECT_EQ(payload[0], uint8_t(h264::NaluType::STAP_A));
    EXPECT_EQ(payload.size(), kNalHeaderSize + 3 * kLengthFieldLength + 2 + 2 + 0x123);

    payload = payload.subview(kNalHeaderSize);
    EXPECT_EQ(payload.size(), 3 * kLengthFieldLength + 2 + 2 + 0x123);
    // 1st fragment.
    auto fragment_header = payload.subview(0, kLengthFieldLength);
    EXPECT_EQ(fragment_header.data()[0], 0);
    EXPECT_EQ(fragment_header.data()[1], 2);
    auto fragment_payload = payload.subview(kLengthFieldLength, 2);
    EXPECT_THAT(fragment_payload, ::testing::ElementsAreArray(nalus[0]));
    payload = payload.subview(kLengthFieldLength + 2);
    // 2nd fragment.
    fragment_header = payload.subview(0, kLengthFieldLength);
    EXPECT_EQ(fragment_header.data()[0], 0);
    EXPECT_EQ(fragment_header.data()[1], 2);
    fragment_payload = payload.subview(kLengthFieldLength, 2);
    EXPECT_THAT(fragment_payload, ::testing::ElementsAreArray(nalus[1]));
    payload = payload.subview(kLengthFieldLength + 2);
    // 3rd fragment.
    fragment_header = payload.subview(0, kLengthFieldLength);
    EXPECT_EQ(fragment_header.data()[0], 0x01);
    EXPECT_EQ(fragment_header.data()[1], 0x23);
    fragment_payload = payload.subview(kLengthFieldLength);
    EXPECT_THAT(fragment_payload, ::testing::ElementsAreArray(nalus[2]));
}

TEST(RtpH264PacketizerTest, SingleNalUnitModeHasNoStapA) {
    // This is the same setup as for the StapA test.
    BinaryBuffer nalus[] = {GenerateNalUnit(/*size=*/2),
                            GenerateNalUnit(/*size=*/2),
                            GenerateNalUnit(/*size=*/0x123)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpPacketizer::PayloadSizeLimits limits;
    RtpH264Packetizer packetizer(frame, limits, h264::PacketizationMode::SINGLE_NAL_UNIT);
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    // The three fragments should be returned as three packets.
    ASSERT_THAT(packets, ::testing::SizeIs(3));
    EXPECT_EQ(packets[0].payload_size(), 2u);
    EXPECT_EQ(packets[1].payload_size(), 2u);
    EXPECT_EQ(packets[2].payload_size(), 0x123u);
}

TEST(RtpH264PacketizerTest, StapARespectsFirstPacketReduction) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1000;
    limits.first_packet_reduction_size = 100;
    const size_t kFirstFragmentSize = limits.max_payload_size - limits.first_packet_reduction_size;
    BinaryBuffer nalus[] = {GenerateNalUnit(/*size=*/kFirstFragmentSize),
                           GenerateNalUnit(/*size=*/2),
                           GenerateNalUnit(/*size=*/2)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpH264Packetizer packetizer(frame, limits, h264::PacketizationMode::NON_INTERLEAVED);
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    ASSERT_THAT(packets, ::testing::SizeIs(2));
    // Expect 1st packet is single nalu.
    EXPECT_THAT(packets[0].payload(), ::testing::ElementsAreArray(nalus[0]));
    // Expect 2nd packet is aggregate of last two fragments.
    EXPECT_THAT(packets[1].payload(), ::testing::ElementsAre(uint8_t(h264::NaluType::STAP_A),
                                                             0, 2, nalus[1][0], nalus[1][1], 
                                                             0, 2, nalus[2][0], nalus[2][1]));
}

TEST(RtpH264PacketizerTest, StapARespectsLastPacketReduction) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1000;
    limits.last_packet_reduction_size = 100;
    const size_t kLastFragmentSize = limits.max_payload_size - limits.last_packet_reduction_size;
    BinaryBuffer nalus[] = {GenerateNalUnit(/*size=*/2),
                           GenerateNalUnit(/*size=*/2),
                           GenerateNalUnit(/*size=*/kLastFragmentSize)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpH264Packetizer packetizer(frame, limits, h264::PacketizationMode::NON_INTERLEAVED);
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    ASSERT_THAT(packets, ::testing::SizeIs(2));
    // Expect 1st packet is aggregate of 1st two fragments.
    EXPECT_THAT(packets[0].payload(),
                ::testing::ElementsAre(uint8_t(h264::NaluType::STAP_A),
                                       0, 2, nalus[0][0], nalus[0][1],
                                       0, 2, nalus[1][0], nalus[1][1]));
    // Expect 2nd packet is single nalu.
    EXPECT_THAT(packets[1].payload(), ::testing::ElementsAreArray(nalus[2]));
}

TEST(RtpH264PacketizerTest, TooSmallForStapAHeaders) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1000;
    const size_t kLastFragmentSize = limits.max_payload_size - 3 * kLengthFieldLength - 4;
    BinaryBuffer nalus[] = {GenerateNalUnit(/*size=*/2),
                            GenerateNalUnit(/*size=*/2),
                            GenerateNalUnit(/*size=*/kLastFragmentSize)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpH264Packetizer packetizer(frame, limits, h264::PacketizationMode::NON_INTERLEAVED);
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    ASSERT_THAT(packets, ::testing::SizeIs(2));
    // Expect 1st packet is aggregate of 1st two fragments.
    EXPECT_THAT(packets[0].payload(),
                ::testing::ElementsAre(uint8_t(h264::NaluType::STAP_A),
                                        0, 2, nalus[0][0], nalus[0][1],
                                        0, 2, nalus[1][0], nalus[1][1]));
    // Expect 2nd packet is single nalu.
    EXPECT_THAT(packets[1].payload(), ::testing::ElementsAreArray(nalus[2]));
}

// Fragmentation + aggregation.
TEST(RtpH264PacketizerTest, MixedStapAFUA) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 100;
    const size_t kFuaPayloadSize = 70;
    const size_t kFuaNaluSize = kNalHeaderSize + 2 * kFuaPayloadSize;
    const size_t kStapANaluSize = 20;
    BinaryBuffer nalus[] = {GenerateNalUnit(kFuaNaluSize),
                            GenerateNalUnit(kStapANaluSize),
                            GenerateNalUnit(kStapANaluSize)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpH264Packetizer packetizer(frame, limits, h264::PacketizationMode::NON_INTERLEAVED);
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    ASSERT_THAT(packets, ::testing::SizeIs(3));
    // First expect two FU-A packets.
    EXPECT_THAT(packets[0].payload().subview(0, kFuAHeaderSize),
                ::testing::ElementsAre(uint8_t(h264::NaluType::FU_A), 0x80 /* FU-A indicator with forbidden bit set*/ | nalus[0][0]));
    EXPECT_THAT(packets[0].payload().subview(kFuAHeaderSize),
                ::testing::ElementsAreArray(nalus[0].data() + kNalHeaderSize, kFuaPayloadSize));

    EXPECT_THAT(packets[1].payload().subview(0, kFuAHeaderSize),
                ::testing::ElementsAre(uint8_t(h264::NaluType::FU_A), 0x40 /* FU-A Header with E bit */ | nalus[0][0]));
    EXPECT_THAT(packets[1].payload().subview(kFuAHeaderSize),
                ::testing::ElementsAreArray(nalus[0].data() + kNalHeaderSize + kFuaPayloadSize, kFuaPayloadSize));

    // Then expect one STAP-A packet with two nal units.
    EXPECT_THAT(packets[2].payload()[0], uint8_t(h264::NaluType::STAP_A));
    auto payload = packets[2].payload().subview(kNalHeaderSize);
    EXPECT_THAT(payload.subview(0, kLengthFieldLength),
                ::testing::ElementsAre(0, kStapANaluSize));
    EXPECT_THAT(payload.subview(kLengthFieldLength, kStapANaluSize),
                ::testing::ElementsAreArray(nalus[1]));
    payload = payload.subview(kLengthFieldLength + kStapANaluSize);
    EXPECT_THAT(payload.subview(0, kLengthFieldLength),
                ::testing::ElementsAre(0, kStapANaluSize));
    EXPECT_THAT(payload.subview(kLengthFieldLength), 
                ::testing::ElementsAreArray(nalus[2]));
}

TEST(RtpH264PacketizerTest, LastFragmentFitsInSingleButNotLastPacket) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1178;
    limits.first_packet_reduction_size = 0;
    limits.last_packet_reduction_size= 20;
    limits.single_packet_reduction_size = 20;
    // Actual sizes, which triggered this bug.
    BinaryBuffer nalus[] = {GenerateNalUnit(20),
                            GenerateNalUnit(8),
                            GenerateNalUnit(18),
                            GenerateNalUnit(1161)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpH264Packetizer packetizer(frame, limits, h264::PacketizationMode::NON_INTERLEAVED);
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    // Last packet has to be of correct size.
    // Incorrect implementation might miss this constraint and not split the last
    // fragment in two packets.
    EXPECT_LE(static_cast<int>(packets.back().payload_size()),
                limits.max_payload_size - limits.last_packet_reduction_size);
}

// Splits frame with payload size |frame_payload_size| without fragmentation,
// Returns sizes of the payloads excluding fua headers.
std::vector<int> TestFua(size_t frame_payload_size,
                         const RtpPacketizer::PayloadSizeLimits& limits) {
    BinaryBuffer nalu[] = {GenerateNalUnit(kNalHeaderSize + frame_payload_size)};
    BinaryBuffer frame = CreateFrame(nalu);

    RtpH264Packetizer packetizer(frame, limits, h264::PacketizationMode::NON_INTERLEAVED);
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    EXPECT_GE(packets.size(), 2u);  // Single packet indicates it is not FuA.
    std::vector<uint16_t> fua_header;
    std::vector<int> payload_sizes;

    for (const RtpPacketToSend& packet : packets) {
        auto payload = packet.payload();
        EXPECT_GT(payload.size(), kFuAHeaderSize);
        fua_header.push_back((payload[0] << 8) | payload[1]);
        payload_sizes.push_back(payload.size() - kFuAHeaderSize);
    }

    EXPECT_TRUE(fua_header.front() & 0x80 /* FU-A header with start bit set */);
    EXPECT_TRUE(fua_header.back() & 0x40 /* FU-A header with end bit set */);
    // Clear S and E bits before testing all are duplicating same original header.
    fua_header.front() &= ~0x80;
    fua_header.back() &= ~0x40;
    EXPECT_THAT(fua_header, ::testing::Each(::testing::Eq((uint8_t(h264::NaluType::FU_A) << 8) | nalu[0][0])));

    return payload_sizes;
}

// Fragmentation tests.
TEST(RtpH264PacketizerTest, FUAOddSize) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1200;
    EXPECT_THAT(TestFua(1200, limits), ::testing::ElementsAre(600, 600));
}

TEST(RtpH264PacketizerTest, FUAWithFirstPacketReduction) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1200;
    limits.first_packet_reduction_size = 4;
    limits.single_packet_reduction_size = 4;
    EXPECT_THAT(TestFua(1198, limits), ::testing::ElementsAre(597, 601));
}

TEST(RtpH264PacketizerTest, FUAWithLastPacketReduction) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1200;
    limits.last_packet_reduction_size = 4;
    limits.single_packet_reduction_size = 4;
    EXPECT_THAT(TestFua(1198, limits), ::testing::ElementsAre(601, 597));
}

TEST(RtpH264PacketizerTest, FUAWithSinglePacketReduction) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1199;
    limits.single_packet_reduction_size = 200;
    EXPECT_THAT(TestFua(1000, limits), ::testing::ElementsAre(500, 500));
}

TEST(RtpH264PacketizerTest, FUAEvenSize) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1200;
    EXPECT_THAT(TestFua(1201, limits), ::testing::ElementsAre(600, 601));
}

TEST(RtpH264PacketizerTest, FUARounding) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1448;
    EXPECT_THAT(TestFua(10123, limits),
                ::testing::ElementsAre(1265, 1265, 1265, 1265, 1265, 1266, 1266, 1266));
}

TEST(RtpH264PacketizerTest, FUABig) {
    RtpPacketizer::PayloadSizeLimits limits;
    limits.max_payload_size = 1200;
    // Generate 10 full sized packets, leave room for FU-A headers.
    EXPECT_THAT(TestFua(10 * (1200 - kFuAHeaderSize), limits),
                ::testing::ElementsAre(1198, 1198, 1198, 1198, 1198, 1198, 1198, 1198, 1198, 1198));
}

TEST(RtpH264PacketizerTest, RejectsOverlongDataInPacketizationMode0) {
    RtpPacketizer::PayloadSizeLimits limits;
    BinaryBuffer nalus[] = {GenerateNalUnit(kMaxPayloadSize + 1)};
    BinaryBuffer frame = CreateFrame(nalus);

    RtpH264Packetizer packetizer(frame, limits, h264::PacketizationMode::SINGLE_NAL_UNIT);
    std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

    EXPECT_THAT(packets, ::testing::IsEmpty());
}

} // namespace test
} // namespace naivertc
