#include "rtc/media/video/codecs/h264/nalunit.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

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


MY_TEST(NalUnitTest, Create) {
    NalUnit nalu;
    EXPECT_EQ(1u, nalu.size());

    nalu.set_forbidden_bit(false);
    nalu.set_nri(0x03);
    nalu.set_unit_type(0x0F);
    nalu.set_payload(&kPacket[1], kPacketSize - 1);

    EXPECT_FALSE(nalu.forbidden_bit());
    EXPECT_EQ(0x03, nalu.nri());
    EXPECT_EQ(0x0F, nalu.unit_type());

    EXPECT_THAT(nalu, testing::ElementsAreArray(kPacket));
}

MY_TEST(NalUnitTest, Parse) {
    NalUnit nalu(kPacket, kPacketSize);

    EXPECT_FALSE(nalu.forbidden_bit());
    EXPECT_EQ(0x03, nalu.nri());
    EXPECT_EQ(0x0F, nalu.unit_type());

    EXPECT_THAT(nalu.payload(), testing::ElementsAreArray(&kPacket[1], kPacketSize - 1));

}

MY_TEST(NalUnitTest, FindNaluIndicesWithShortStartSequence) {
    const uint8_t h264_encoded_buffer[] = {0, 0, 1, uint8_t(h264::NaluType::IDR), 0xFF};

    std::vector<NaluIndex> nalu_indices = NalUnit::FindNaluIndices(h264_encoded_buffer, 5);
    EXPECT_EQ(nalu_indices.size(), 1u);
    EXPECT_EQ(nalu_indices[0].start_offset, 0);
    EXPECT_EQ(nalu_indices[0].payload_start_offset, 3);
    EXPECT_EQ(nalu_indices[0].payload_size, 2);
}

MY_TEST(NalUnitTest, FindNaluIndicesWithLongStartSequence) {
    const uint8_t h264_encoded_buffer[] = {0, 0, 0, 1, uint8_t(h264::NaluType::IDR), 0xFF};

    std::vector<NaluIndex> nalu_indices = NalUnit::FindNaluIndices(h264_encoded_buffer, 6, NalUnit::Separator::LONG_START_SEQUENCE);
    EXPECT_EQ(nalu_indices.size(), 1u);
    EXPECT_EQ(nalu_indices[0].start_offset, 0);
    EXPECT_EQ(nalu_indices[0].payload_start_offset, 4);
    EXPECT_EQ(nalu_indices[0].payload_size, 2);
}

MY_TEST(NalUnitTest, FindNaluIndicesWithLengthSeparator) {
    const uint8_t h264_encoded_buffer[] = {0x00,0x00,0x00,0x19,0x67,0x42,0xc0,0x1f,
                                           0xd9,0x80,0x50,0x05,0xbb,0x01,0x10,0x00,
                                           0x00,0x03,0x00,0x10,0x00,0x00,0x03,0x03,
                                           0xc0,0xf1,0x83,0x26,0x80,
                                           0x00,0x00,0x00,0x05,0x68,0xc9,0x60,0xf2,
                                           0xc8};
    const size_t kBufferSize = sizeof(h264_encoded_buffer);

    std::vector<NaluIndex> nalu_indices = NalUnit::FindNaluIndices(h264_encoded_buffer, kBufferSize, NalUnit::Separator::LENGTH);
    EXPECT_EQ(nalu_indices.size(), 2u);
    EXPECT_EQ(nalu_indices[0].start_offset, 0);
    EXPECT_EQ(nalu_indices[0].payload_start_offset, 4);
    EXPECT_EQ(nalu_indices[0].payload_size, 25);
    EXPECT_EQ(nalu_indices[1].start_offset, 29);
    EXPECT_EQ(nalu_indices[1].payload_start_offset, 33);
    EXPECT_EQ(nalu_indices[1].payload_size, 5);
}

MY_TEST(NalUnitTest, RetrieveRbspFromEbsp) {
    uint8_t ebsp_buffer_1[] = {0x00, 0x00, 0x03, 0x01};
    uint8_t ebsp_buffer_2[] = {0x00, 0x00, 0x03, 0x02};
    uint8_t ebsp_buffer_3[] = {0x00, 0x00, 0x03, 0x03};

    auto rbsp_buffer_1 = NalUnit::RetrieveRbspFromEbsp(ebsp_buffer_1, 4);
    EXPECT_EQ(rbsp_buffer_1.size(), 3);
    EXPECT_EQ(rbsp_buffer_1[0], 0x00);
    EXPECT_EQ(rbsp_buffer_1[1], 0x00);
    EXPECT_EQ(rbsp_buffer_1[2], 0x01);

    auto rbsp_buffer_2 = NalUnit::RetrieveRbspFromEbsp(ebsp_buffer_2, 4);
    EXPECT_EQ(rbsp_buffer_2.size(), 3);
    EXPECT_EQ(rbsp_buffer_2[0], 0x00);
    EXPECT_EQ(rbsp_buffer_2[1], 0x00);
    EXPECT_EQ(rbsp_buffer_2[2], 0x02);

    auto rbsp_buffer_3 = NalUnit::RetrieveRbspFromEbsp(ebsp_buffer_3, 4);
    EXPECT_EQ(rbsp_buffer_3.size(), 3);
    EXPECT_EQ(rbsp_buffer_3[0], 0x00);
    EXPECT_EQ(rbsp_buffer_3[1], 0x00);
    EXPECT_EQ(rbsp_buffer_3[2], 0x03);
    
}
    
} // namespace test
} // namespace naivertc
