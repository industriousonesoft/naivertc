#include "rtc/media/video/h264/nalunit.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace naivertc::h264;

namespace naivertc {
namespace test {

namespace {
constexpr uint8_t kPacket[] = {
    0x6F, 0x12, 0x34, 0x56,
    0x78, 0x9a, 0x21, 0x22,
    0x23, 0x24};
constexpr size_t kPacketSize = sizeof(kPacket);

} // namespace


TEST(H264NalUnitTest, Create) {
    NalUnit nalu;
    EXPECT_EQ(1u, nalu.size());

    nalu.set_forbidden_bit(false);
    nalu.set_nri(0x03);
    nalu.set_unit_type(0x0F);
    nalu.set_payload(&kPacket[1], kPacketSize - 1);

    EXPECT_FALSE(nalu.forbidden_bit());
    EXPECT_EQ(0x03, nalu.nri());
    EXPECT_EQ(0x0F, nalu.unit_type());

    EXPECT_THAT(std::make_tuple(nalu.data(), nalu.size()), testing::ElementsAreArray(kPacket));
}

TEST(H264NalUnitTest, Parse) {
    NalUnit nalu(kPacket, kPacketSize);

    EXPECT_FALSE(nalu.forbidden_bit());
    EXPECT_EQ(0x03, nalu.nri());
    EXPECT_EQ(0x0F, nalu.unit_type());

    EXPECT_THAT(std::make_tuple(nalu.payload().data(), nalu.payload().size()), testing::ElementsAreArray(&kPacket[1], kPacketSize - 1));

}

TEST(H264NalUnitTest, FindNaluIndices) {
    const uint8_t h264_encoded_buffer[] = {0, 0, 1, uint8_t(h264::NaluType::IDR), 0xFF};

    std::vector<NaluIndex> nalu_indices = NalUnit::FindNaluIndices(h264_encoded_buffer, sizeof(h264_encoded_buffer));
    EXPECT_EQ(nalu_indices.size(), 1u);
    EXPECT_EQ(nalu_indices[0].start_offset, 0);
    EXPECT_EQ(nalu_indices[0].payload_start_offset, 3);
    EXPECT_EQ(nalu_indices[0].payload_size, 2);
}
    
} // namespace test
} // namespace naivertc
