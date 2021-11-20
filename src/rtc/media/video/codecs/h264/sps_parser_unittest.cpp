#include "rtc/media/video/codecs/h264/sps_parser.hpp"
#include "rtc/media/video/codecs/h264/nalunit.hpp"
#include "rtc/base/memory/bit_io_writer.hpp"
#include "rtc/base/memory/bit_io_reader.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

namespace naivertc {
namespace test {
// Example SPS can be generated with ffmpeg. Here's an example set of commands,
// runnable on OS X:
// 1) Generate a video, from the camera:
// ffmpeg -f avfoundation -i "0" -video_size 640x360 camera.mov
//
// 2) Scale the video to the desired size:
// ffmpeg -i camera.mov -vf scale=640x360 scaled.mov
//
// 3) Get just the H.264 bitstream in AnnexB:
// ffmpeg -i scaled.mov -vcodec copy -vbsf h264_mp4toannexb -an out.h264
//
// 4) Open out.h264 and find the SPS, generally everything between the first
// two start codes (0 0 0 1 or 0 0 1). The first byte should be 0x67,
// which should be stripped out before being passed to the parser.

static const size_t kSpsBufferMaxSize = 256;

// Generates a fake SPS with basically everything empty but the width/height.
// Pass in a buffer of at least kSpsBufferMaxSize.
// The fake SPS that this generates also always has at least one emulation byte
// at offset 2, since the first two bytes are always 0, and has a 0x3 as the
// level_idc, to make sure the parser doesn't eat all 0x3 bytes.
void GenerateFakeSps(uint16_t width,
                     uint16_t height,
                     int id,
                     uint32_t log2_max_frame_num_minus4,
                     uint32_t log2_max_pic_order_cnt_lsb_minus4,
                     std::vector<uint8_t>& out_buffer) {
    uint8_t rbsp[kSpsBufferMaxSize] = {0};
    BitWriter bit_writer(rbsp, kSpsBufferMaxSize);
    
    // Profile byte.
    uint8_t profile_byte = 0;
    bit_writer.WriteByte(profile_byte);
    // Constraint sets and reserved zero bits.
    uint8_t ignored = 0;
    bit_writer.WriteByte(ignored);
    // level_idc.
    uint8_t level_idc = 0x3u;
    bit_writer.WriteByte(level_idc);
    // seq_paramter_set_id.
    bit_writer.WriteExpGolomb(id);
    // Profile is not special, so we skip all the chroma format settings.

    // Now some bit magic.
    // log2_max_frame_num_minus4: ue(v).
    bit_writer.WriteExpGolomb(log2_max_frame_num_minus4);
    // pic_order_cnt_type: ue(v). 0 is the type we want.
    bit_writer.WriteExpGolomb(0);
    // log2_max_pic_order_cnt_lsb_minus4: ue(v). 0 is fine.
    bit_writer.WriteExpGolomb(log2_max_pic_order_cnt_lsb_minus4);
    // max_num_ref_frames: ue(v). 0 is fine.
    bit_writer.WriteExpGolomb(0);
    // gaps_in_frame_num_value_allowed_flag: u(1).
    bit_writer.WriteBits(0, 1);
    // Next are width/height. First, calculate the mbs/map_units versions.
    uint16_t width_in_mbs_minus1 = (width + 15) / 16 - 1;

    // For the height, we're going to define frame_mbs_only_flag, so we need to
    // divide by 2. See the parser for the full calculation.
    uint16_t height_in_map_units_minus1 = ((height + 15) / 16 - 1) / 2;
    // Write each as ue(v).
    bit_writer.WriteExpGolomb(width_in_mbs_minus1);
    bit_writer.WriteExpGolomb(height_in_map_units_minus1);
    // frame_mbs_only_flag: u(1). Needs to be false.
    bit_writer.WriteBits(0, 1);
    // mb_adaptive_frame_field_flag: u(1).
    bit_writer.WriteBits(0, 1);
    // direct_8x8_inferene_flag: u(1).
    bit_writer.WriteBits(0, 1);
    // frame_cropping_flag: u(1). 1, so we can supply crop.
    bit_writer.WriteBits(1, 1);
    // Now we write the left/right/top/bottom crop. For simplicity, we'll put all
    // the crop at the left/top.
    // We picked a 4:2:0 format, so the crops are 1/2 the pixel crop values.
    // Left/right.
    bit_writer.WriteExpGolomb(((16 - (width % 16)) % 16) / 2);
    bit_writer.WriteExpGolomb(0);
    // Top/bottom.
    bit_writer.WriteExpGolomb(((16 - (height % 16)) % 16) / 2);
    bit_writer.WriteExpGolomb(0);

    // vui_parameters_present_flag: u(1)
    bit_writer.WriteBits(0, 1);

    // Get the number of bytes written (including the last partial byte).
    size_t byte_count, bit_offset;
    bit_writer.GetCurrentOffset(&byte_count, &bit_offset);
    if (bit_offset > 0) {
        byte_count++;
    }

    out_buffer.clear();
    h264::NalUnit::WriteRbsp(rbsp, byte_count, out_buffer);
}

MY_TEST(SpsParserTest, TestSampleSPSHdLandscape) {
    // SPS for a 1280x720 camera capture from ffmpeg on osx. Contains
    // emulation bytes but no cropping.
    const uint8_t buffer[] = {0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50, 0x05,
                              0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0, 0x00,
                              0x00, 0x2A, 0xE0, 0xF1, 0x83, 0x19, 0x60};
    std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer, 23);
    EXPECT_TRUE(sps.has_value());
    EXPECT_EQ(1280u, sps->width);
    EXPECT_EQ(720u, sps->height);
}

MY_TEST(SpsParserTest, TestSampleSPSWeirdResolution) {
    // SPS for a 200x400 camera capture from ffmpeg on osx. Horizontal and
    // veritcal crop (neither dimension is divisible by 16).
    const uint8_t buffer[] = {0x7A, 0x00, 0x0D, 0xBC, 0xD9, 0x43, 0x43, 0x3E,
                              0x5E, 0x10, 0x00, 0x00, 0x03, 0x00, 0x60, 0x00,
                              0x00, 0x15, 0xA0, 0xF1, 0x42, 0x99, 0x60};
    std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer, 23);
    EXPECT_TRUE(sps.has_value());
    EXPECT_EQ(200u, sps->width);
    EXPECT_EQ(400u, sps->height);
}

MY_TEST(SpsParserTest, TestSyntheticSPSQvgaLandscape) {
    std::vector<uint8_t> buffer;
    GenerateFakeSps(320u, 180u, 1, 0, 0, buffer);
    std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer.data(), buffer.size());
    EXPECT_TRUE(sps.has_value());
    EXPECT_EQ(320u, sps->width);
    EXPECT_EQ(180u, sps->height);
    EXPECT_EQ(1u, sps->id);
}

MY_TEST(SpsParserTest, TestLog2MaxFrameNumMinus4) {
    std::vector<uint8_t> buffer;
    GenerateFakeSps(320u, 180u, 1, 0, 0, buffer);
    std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer.data(), buffer.size());
    EXPECT_TRUE(sps.has_value());
    EXPECT_EQ(320u, sps->width);
    EXPECT_EQ(180u, sps->height);
    EXPECT_EQ(1u, sps->id);
    EXPECT_EQ(4u, sps->log2_max_frame_num);

    GenerateFakeSps(320u, 180u, 1, 28, 0, buffer);
    sps = SpsParser::ParseSps(buffer.data(), buffer.size());
    EXPECT_TRUE(sps.has_value());
    EXPECT_EQ(320u, sps->width);
    EXPECT_EQ(180u, sps->height);
    EXPECT_EQ(1u, sps->id);
    EXPECT_EQ(32u, sps->log2_max_frame_num);

    GenerateFakeSps(320u, 180u, 1, 29, 0, buffer);
    sps = SpsParser::ParseSps(buffer.data(), buffer.size());
    EXPECT_FALSE(sps.has_value());
}

MY_TEST(SpsParserTest, TestLog2MaxPicOrderCntMinus4) {
    std::vector<uint8_t> buffer;
    GenerateFakeSps(320u, 180u, 1, 0, 0, buffer);
    std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer.data(), buffer.size());
    EXPECT_TRUE(sps.has_value());
    EXPECT_EQ(320u, sps->width);
    EXPECT_EQ(180u, sps->height);
    EXPECT_EQ(1u, sps->id);
    EXPECT_EQ(4u, sps->log2_max_pic_order_cnt_lsb);

    GenerateFakeSps(320u, 180u, 1, 0, 28, buffer);
    sps = SpsParser::ParseSps(buffer.data(), buffer.size());
    EXPECT_TRUE(sps.has_value());
    EXPECT_EQ(320u, sps->width);
    EXPECT_EQ(180u, sps->height);
    EXPECT_EQ(1u, sps->id);
    EXPECT_EQ(32u, sps->log2_max_pic_order_cnt_lsb);

    GenerateFakeSps(320u, 180u, 1, 0, 29, buffer);
    sps = SpsParser::ParseSps(buffer.data(), buffer.size());
    EXPECT_FALSE(sps.has_value());
}

} // namespace test
} // namespace naivertc