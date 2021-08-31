#include "rtc/rtp_rtcp/rtp/fec/fec_header_writer_ulp.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"
#include "common/utils_random.hpp"
#include "rtc/base/byte_io_reader.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace naivertc {
namespace test {
namespace {

constexpr size_t kMediaPacketSize = 1234;
constexpr uint32_t kMediaSsrc = 1254983;
constexpr uint16_t kMediaStartSeqNum = 825;

constexpr size_t kUlpfecHeaderSizeLBitClear = 14;
constexpr size_t kUlpfecHeaderSizeLBitSet = 18;
constexpr size_t kUlpfecPacketMaskOffset = 12;

std::unique_ptr<uint8_t[]> GeneratePacketMask(size_t packet_mask_size, uint64_t seed) {
    std::unique_ptr<uint8_t[]> packet_mask(new uint8_t[packet_mask_size]);
    for (size_t i = 0; i < packet_mask_size; ++i) {
        packet_mask[i] = utils::random::generate_random<uint8_t>();
    }
    return packet_mask;
}

std::unique_ptr<FecPacket> WriteHeader(const uint8_t* packet_mask, size_t packet_mask_size) {
    std::unique_ptr<FecPacket> written_packet(new FecPacket());
    written_packet->resize(kMediaPacketSize);
    uint8_t* data = written_packet->data();
    for (size_t i = 0; i < written_packet->size(); ++i) {
        // Actual content dosen't matter
        data[i] = i;
    }
    UlpfecHeaderWriter writer;
    writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask, packet_mask_size, written_packet.get());
    return written_packet;
}
    
} // namespace

TEST(UlpfecHeaderWriterTest, FinalizeSmallHeader) {
    const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
    auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
    FecPacket written_packet;
    written_packet.resize(kMediaPacketSize);
    uint8_t* data = written_packet.data();
    for (size_t i = 0; i < written_packet.size(); ++i) {
        data[i] = i;
    }

    UlpfecHeaderWriter writer;
    writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask.get(), packet_mask_size, &written_packet);

    const uint8_t* packet_data = written_packet.data();
    EXPECT_EQ(0x00, packet_data[0] & 0x80); // E bit
    EXPECT_EQ(0x00, packet_data[0] & 0x40); // L bit
    EXPECT_EQ(kMediaStartSeqNum, ByteReader<uint16_t>::ReadBigEndian(packet_data + 2));
    EXPECT_EQ(static_cast<uint16_t>(kMediaPacketSize - kUlpfecHeaderSizeLBitClear), ByteReader<uint16_t>::ReadBigEndian(packet_data + 10));
    EXPECT_EQ(0, memcmp(packet_data + kUlpfecPacketMaskOffset, packet_mask.get(), packet_mask_size));
}


TEST(UlpfecHeaderWriterTest, FinalizeLargeHeader) {
    const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
    auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);

    FecPacket written_packet;
    written_packet.resize(kMediaPacketSize);
    uint8_t* data = written_packet.data();
    for (size_t i = 0; i < written_packet.size(); ++i) {
        data[i] = i;
    }

    UlpfecHeaderWriter writer;
    writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask.get(), packet_mask_size, &written_packet);

    const uint8_t* packet_data = written_packet.data();
    EXPECT_EQ(0x00, packet_data[0] & 0x80); // E bit
    EXPECT_EQ(0x40, packet_data[0] & 0x40); // L bit
    EXPECT_EQ(kMediaStartSeqNum, ByteReader<uint16_t>::ReadBigEndian(packet_data + 2));
    EXPECT_EQ(static_cast<uint16_t>(kMediaPacketSize - kUlpfecHeaderSizeLBitSet), ByteReader<uint16_t>::ReadBigEndian(packet_data + 10));
    EXPECT_EQ(0, memcmp(packet_data + kUlpfecPacketMaskOffset, packet_mask.get(), packet_mask_size));
}

TEST(UlpfecHeaderWriterTest, CalculateSmallHeaderSize) {
    const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
    auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);

    UlpfecHeaderWriter writer;
    size_t min_packet_mask_size = writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

    EXPECT_EQ(kUlpfecPacketMaskSizeLBitClear, min_packet_mask_size);
    EXPECT_EQ(kUlpfecHeaderSizeLBitClear, writer.FecHeaderSize(min_packet_mask_size));
}

TEST(UlpfecHeaderWriterTest, CalculateLargeHeaderSize) {
    const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
    auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);

    UlpfecHeaderWriter writer;
    size_t min_packet_mask_size = writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

    EXPECT_EQ(kUlpfecPacketMaskSizeLBitSet, min_packet_mask_size);
    EXPECT_EQ(kUlpfecHeaderSizeLBitSet, writer.FecHeaderSize(min_packet_mask_size));
}
    
} // namespace test
} // namespace naivertc
