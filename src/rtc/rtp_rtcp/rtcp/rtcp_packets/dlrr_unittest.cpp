#include "rtc/rtp_rtcp/rtcp/rtcp_packets/dlrr.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

#include <gtest/gtest.h>

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {
namespace {
const uint32_t kSsrc = 0x12345678;
const uint32_t kLastRR = 0x23344556;
const uint32_t kDelay = 0x33343536;
const uint8_t kBlock[] = {0x05, 0x00, 0x00, 0x03, 0x12, 0x34, 0x56, 0x78,
                          0x23, 0x34, 0x45, 0x56, 0x33, 0x34, 0x35, 0x36};
const size_t kBlockSize = sizeof(kBlock);
}  // namespace

TEST(RTP_RTCP_RtcpPacketDlrrTest, Empty) {
    Dlrr dlrr;
    EXPECT_EQ(0u, dlrr.BlockSize());
}

TEST(RTP_RTCP_RtcpPacketDlrrTest, Pack) {
    Dlrr dlrr;
    dlrr.AddDlrrItem(ReceiveTimeInfo(kSsrc, kLastRR, kDelay));

    ASSERT_EQ(kBlockSize, dlrr.BlockSize());
    uint8_t buffer[kBlockSize];

    dlrr.PackInto(buffer, kBlockSize);
    EXPECT_EQ(0, memcmp(buffer, kBlock, kBlockSize));
}

TEST(RTP_RTCP_RtcpPacketDlrrTest, Parse) {
    Dlrr dlrr;
    uint16_t block_size = ByteReader<uint16_t>::ReadBigEndian(&kBlock[2]);
    EXPECT_TRUE(dlrr.Parse(kBlock, block_size));

    EXPECT_EQ(1u, dlrr.sub_blocks().size());
    const ReceiveTimeInfo& block = dlrr.sub_blocks().front();
    EXPECT_EQ(kSsrc, block.ssrc);
    EXPECT_EQ(kLastRR, block.last_rr);
    EXPECT_EQ(kDelay, block.delay_since_last_rr);
}

TEST(RTP_RTCP_RtcpPacketDlrrTest, ParseFailsOnBadSize) {
    const size_t kBigBufferSize = 0x100;  // More than enough.
    uint8_t buffer[kBigBufferSize];
    buffer[0] = Dlrr::kBlockType;
    buffer[1] = 0;  // Reserved.
    buffer[2] = 0;  // Most significant size byte.
    for (uint8_t size = 3; size < 6; ++size) {
        buffer[3] = size;
        Dlrr dlrr;
        // Parse should be successful only when size is multiple of 3.
        EXPECT_EQ(size % 3 == 0, dlrr.Parse(buffer, static_cast<uint16_t>(size)));
    }
}

TEST(RTP_RTCP_RtcpPacketDlrrTest, CreateAndParseManySubBlocks) {
    const size_t kBufferSize = 0x1000;  // More than enough.
    const size_t kManyDlrrItems = 50;
    uint8_t buffer[kBufferSize];

    // Create.
    Dlrr dlrr;
    for (size_t i = 1; i <= kManyDlrrItems; ++i)
        dlrr.AddDlrrItem(ReceiveTimeInfo(kSsrc + i, kLastRR + i, kDelay + i));
    size_t used_buffer_size = dlrr.BlockSize();
    ASSERT_LE(used_buffer_size, kBufferSize);
    dlrr.PackInto(buffer, kBufferSize);

    // Parse.
    Dlrr parsed;
    uint16_t block_size = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);
    EXPECT_EQ(used_buffer_size, (block_size + 1) * 4u);
    EXPECT_TRUE(parsed.Parse(buffer, block_size));
    EXPECT_EQ(kManyDlrrItems, parsed.sub_blocks().size());
}

} // namespace rtcp
} // namespace naivertc
