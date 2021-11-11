#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_header_writer_ulp.hpp"
#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_header_reader_ulp.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"
#include "common/utils_random.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_decoder.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

#include <memory>

namespace naivertc {
namespace test {
namespace {

constexpr size_t kMediaPacketSize = 1234;
constexpr uint32_t kMediaSsrc = 1254983;
constexpr uint16_t kMediaStartSeqNum = 825;

constexpr size_t kUlpFecHeaderSizeLBitClear = 14;
constexpr size_t kUlpFecHeaderSizeLBitSet = 18;
constexpr size_t kUlpFecPacketMaskOffset = 12;

using FecPacket = naivertc::FecDecoder::FecPacket;

CopyOnWriteBuffer GeneratePacketMask(size_t packet_mask_size) {
    CopyOnWriteBuffer packet_mask;
    packet_mask.Resize(packet_mask_size);
    uint8_t* packet_mask_data = packet_mask.data();
    for (size_t i = 0; i < packet_mask_size; ++i) {
        packet_mask_data[i] = utils::random::generate_random<uint8_t>();
    }
    return packet_mask;
}

CopyOnWriteBuffer WriteHeader(ArrayView<const uint8_t> packet_mask) {
    CopyOnWriteBuffer written_packet;
    written_packet.Resize(kMediaPacketSize);
    uint8_t* data = written_packet.data();
    for (size_t i = 0; i < written_packet.size(); ++i) {
        // Actual content dosen't matter
        data[i] = i;
    }
    UlpFecHeaderWriter writer;
    writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask.data(), packet_mask.size(), written_packet);
    return written_packet;
}

FecPacket ReadHeader(CopyOnWriteBuffer written_packet) {
    FecPacket read_packet;
    read_packet.ssrc = kMediaSsrc;
    read_packet.pkt = std::move(written_packet);
    read_packet.protected_ssrc = kMediaSsrc;
    UlpFecHeaderReader reader;
    EXPECT_TRUE(reader.ReadFecHeader(read_packet.fec_header, read_packet.pkt));
    return read_packet;
}

void VerifyHeaders(size_t expected_fec_header_size, 
                   ArrayView<const uint8_t> expected_packet_mask,
                   const CopyOnWriteBuffer& written_packet,
                   const FecPacket& read_packet) {
    EXPECT_EQ(kMediaSsrc, read_packet.ssrc);
    EXPECT_EQ(expected_fec_header_size, read_packet.fec_header.fec_header_size);
    EXPECT_EQ(kMediaSsrc, read_packet.protected_ssrc);
    EXPECT_EQ(kMediaStartSeqNum, read_packet.fec_header.seq_num_base);
    EXPECT_EQ(kUlpFecPacketMaskOffset, read_packet.fec_header.packet_mask_offset);
    EXPECT_EQ(expected_packet_mask.size(), read_packet.fec_header.packet_mask_size);
    EXPECT_EQ(written_packet.size() - expected_fec_header_size, read_packet.fec_header.protection_length);
    // Verify packet mask.
    EXPECT_EQ(0, memcmp(expected_packet_mask.data(), read_packet.pkt.data() + read_packet.fec_header.packet_mask_offset, read_packet.fec_header.packet_mask_size));
    // Verify payload data.
    EXPECT_EQ(0, memcmp(written_packet.data() + expected_fec_header_size, read_packet.pkt.data() + expected_fec_header_size, written_packet.size() - expected_fec_header_size));
}
    
} // namespace

MY_TEST(UlpFecHeaderWriterTest, FinalizeSmallHeader) {
    const size_t packet_mask_size = kUlpFecPacketMaskSizeLBitClear;
    CopyOnWriteBuffer packet_mask = GeneratePacketMask(packet_mask_size);
    CopyOnWriteBuffer written_packet;
    written_packet.Resize(kMediaPacketSize);
    uint8_t* data = written_packet.data();
    for (size_t i = 0; i < written_packet.size(); ++i) {
        data[i] = i;
    }

    UlpFecHeaderWriter writer;
    writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask.data(), packet_mask_size, written_packet);

    const uint8_t* packet_data = written_packet.data();
    EXPECT_EQ(0x00, packet_data[0] & 0x80); // E bit
    EXPECT_EQ(0x00, packet_data[0] & 0x40); // L bit
    EXPECT_EQ(kMediaStartSeqNum, ByteReader<uint16_t>::ReadBigEndian(packet_data + 2));
    EXPECT_EQ(static_cast<uint16_t>(kMediaPacketSize - kUlpFecHeaderSizeLBitClear), ByteReader<uint16_t>::ReadBigEndian(packet_data + 10));
    EXPECT_EQ(0, memcmp(packet_data + kUlpFecPacketMaskOffset, packet_mask.data(), packet_mask_size));
}

MY_TEST(UlpFecHeaderWriterTest, FinalizeLargeHeader) {
    const size_t packet_mask_size = kUlpFecPacketMaskSizeLBitSet;
    CopyOnWriteBuffer packet_mask = GeneratePacketMask(packet_mask_size);

    CopyOnWriteBuffer written_packet;
    written_packet.Resize(kMediaPacketSize);
    uint8_t* data = written_packet.data();
    for (size_t i = 0; i < written_packet.size(); ++i) {
        data[i] = i;
    }

    UlpFecHeaderWriter writer;
    writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask.data(), packet_mask_size, written_packet);

    const uint8_t* packet_data = written_packet.data();
    EXPECT_EQ(0x00, packet_data[0] & 0x80); // E bit
    EXPECT_EQ(0x40, packet_data[0] & 0x40); // L bit
    EXPECT_EQ(kMediaStartSeqNum, ByteReader<uint16_t>::ReadBigEndian(packet_data + 2));
    EXPECT_EQ(static_cast<uint16_t>(kMediaPacketSize - kUlpFecHeaderSizeLBitSet), ByteReader<uint16_t>::ReadBigEndian(packet_data + 10));
    EXPECT_EQ(0, memcmp(packet_data + kUlpFecPacketMaskOffset, packet_mask.data(), packet_mask_size));
}

MY_TEST(UlpFecHeaderWriterTest, CalculateSmallHeaderSize) {
    const size_t packet_mask_size = kUlpFecPacketMaskSizeLBitClear;
    auto packet_mask = GeneratePacketMask(packet_mask_size);

    UlpFecHeaderWriter writer;
    size_t min_packet_mask_size = writer.MinPacketMaskSize(packet_mask.data(), packet_mask_size);

    EXPECT_EQ(kUlpFecPacketMaskSizeLBitClear, min_packet_mask_size);
    EXPECT_EQ(kUlpFecHeaderSizeLBitClear, writer.FecHeaderSize(min_packet_mask_size));
}

MY_TEST(UlpFecHeaderWriterTest, CalculateLargeHeaderSize) {
    const size_t packet_mask_size = kUlpFecPacketMaskSizeLBitSet;
    auto packet_mask = GeneratePacketMask(packet_mask_size);

    UlpFecHeaderWriter writer;
    size_t min_packet_mask_size = writer.MinPacketMaskSize(packet_mask.data(), packet_mask_size);

    EXPECT_EQ(kUlpFecPacketMaskSizeLBitSet, min_packet_mask_size);
    EXPECT_EQ(kUlpFecHeaderSizeLBitSet, writer.FecHeaderSize(min_packet_mask_size));
}

MY_TEST(UlpFecHeaderReaderWriterTest, WriteAndReadHeaderWithLBitClear) {
    const size_t packet_mask_size = kUlpFecPacketMaskSizeLBitClear;
    auto packet_mask = GeneratePacketMask(packet_mask_size);
    auto written_packet = WriteHeader(packet_mask);
    auto read_packet = ReadHeader(written_packet);

    VerifyHeaders(kUlpFecHeaderSizeLBitClear, 
                  packet_mask,
                  written_packet, 
                  read_packet);
}

MY_TEST(UlpFecHeaderReaderWriterTest, WriteAndReadHeaderWithLBitSet) {
    const size_t packet_mask_size = kUlpFecPacketMaskSizeLBitSet;
    auto packet_mask = GeneratePacketMask(packet_mask_size);
    auto written_packet = WriteHeader(packet_mask);
    auto read_packet = ReadHeader(written_packet);

    VerifyHeaders(kUlpFecHeaderSizeLBitSet, 
                  packet_mask,
                  written_packet, 
                  read_packet);
}
    
} // namespace test
} // namespace naivertc
