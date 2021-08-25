#include "rtc/media/video/h264/nalunit_fragment.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cmath>

using namespace naivertc::h264;

namespace naivertc {
namespace test {

namespace {
constexpr uint8_t kFragmentPacket[] = {
    0x7C, 0x8F, 0x34, 0x56,
    0x78, 0x9a, 0x21, 0x22,
    0x23, 0x24, 0x25, 0x26};
constexpr size_t kFragmentPacketSize = sizeof(kFragmentPacket);

constexpr uint8_t kPacket[] = {
    0x6F, 0x12, 0x34, 0x56,
    0x78, 0x9a, 0x21, 0x22,
    0x23, 0x24, 0x25, 0x26,
    0x27, 0x28, 0x29, 0x30,
    0x31, 0x32, 0x33, 0x34,
    0x44, 0x45, 0x46, 0x47};
constexpr size_t kPacketSize = sizeof(kPacket);

} // namespace

TEST(H264NaluFragmentTest, Create) {
    NalUnitFragmentA nalu_fragment_a(NalUnitFragmentA::FragmentType::START, false, 0x03, 0x0F, &kFragmentPacket[2], kFragmentPacketSize-2);

    EXPECT_FALSE(nalu_fragment_a.forbidden_bit());
    EXPECT_EQ(0x03u, nalu_fragment_a.nri());
    EXPECT_TRUE(nalu_fragment_a.is_start());
    EXPECT_FALSE(nalu_fragment_a.is_end());
    EXPECT_EQ(0x0Fu, nalu_fragment_a.unit_type());
    EXPECT_EQ(kFragmentPacketSize, nalu_fragment_a.size());
    EXPECT_EQ(kFragmentPacketSize-2, nalu_fragment_a.payload().size());

    EXPECT_THAT(nalu_fragment_a, testing::ElementsAreArray(kFragmentPacket));

}

TEST(H264NaluFragmentTest, ParseFromNalUnit) {
    const size_t kMaxFragmentSize = 12;
    auto nalu = std::make_shared<NalUnit>(kPacket, kPacketSize);

    EXPECT_EQ(kPacketSize, nalu->size());
    EXPECT_EQ(kPacketSize - 1, nalu->payload().size());
    EXPECT_FALSE(nalu->forbidden_bit());
    EXPECT_EQ(0x3u, nalu->nri());
    EXPECT_EQ(0x0Fu, nalu->unit_type());

    auto fragments = NalUnitFragmentA::FragmentsFrom(nalu, kMaxFragmentSize);
    EXPECT_EQ(3u, fragments.size());

    EXPECT_FALSE(fragments[0]->forbidden_bit());
    EXPECT_EQ(0x3u, fragments[0]->nri());
    EXPECT_EQ(0x0Fu, fragments[0]->unit_type());
    EXPECT_EQ(NalUnitFragmentA::FragmentType::START, fragments[0]->fragment_type());
    EXPECT_EQ(10 + 2, fragments[0]->size());
    EXPECT_EQ(0x7C, fragments[0]->at(0));
    EXPECT_EQ(0x8F, fragments[0]->at(1));

    EXPECT_FALSE(fragments[1]->forbidden_bit());
    EXPECT_EQ(0x3u, fragments[1]->nri());
    EXPECT_EQ(0x0Fu, fragments[1]->unit_type());
    EXPECT_EQ(NalUnitFragmentA::FragmentType::MIDDLE, fragments[1]->fragment_type());
    EXPECT_EQ(10 + 2, fragments[1]->size());

    EXPECT_FALSE(fragments[2]->forbidden_bit());
    EXPECT_EQ(0x3u, fragments[2]->nri());
    EXPECT_EQ(0x0Fu, fragments[2]->unit_type());
    EXPECT_EQ(NalUnitFragmentA::FragmentType::END, fragments[2]->fragment_type());
    EXPECT_EQ(3 + 2, fragments[2]->size());
}

    
} // namespace test
} // namespace naivertc
