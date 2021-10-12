#include "rtc/media/video/codecs/h264/pps_parser.hpp"
#include "rtc/media/video/codecs/h264/nalunit.hpp"
#include "rtc/base/bit_io_writer.hpp"
#include "rtc/base/bit_io_reader.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {
namespace {
// Contains enough of the image slice to contain slice QP.
const uint8_t kH264IdrSlice[] = {
    0x65, 0xb7, 0x40, 0xf0, 0x8c, 0x03, 0xf2,
    0x75, 0x67, 0xad, 0x41, 0x64, 0x24, 0x0e, 
    0xa0, 0xb2, 0x12, 0x1e, 0xf8,
};

constexpr size_t kPpsBufferMaxSize = 256;
constexpr uint32_t kIgnored = 0;
}  // namespace

void WritePps(const PpsParser::PpsState& pps,
              int slice_group_map_type,
              int num_slice_groups,
              int pic_size_in_map_units,
              std::vector<uint8_t>& out_buffer) {
    uint8_t data[kPpsBufferMaxSize] = {0};
    BitWriter bit_writer(data, kPpsBufferMaxSize);

    // pic_parameter_set_id: ue(v)
    bit_writer.WriteExpGolomb(pps.id);
    // seq_parameter_set_id: ue(v)
    bit_writer.WriteExpGolomb(pps.sps_id);
    // entropy_coding_mode_flag: u(1)
    bit_writer.WriteBits(pps.entropy_coding_mode_flag, 1);
    // bottom_field_pic_order_in_frame_present_flag: u(1)
    bit_writer.WriteBits(pps.bottom_field_pic_order_in_frame_present_flag ? 1 : 0, 1);

    // num_slice_groups_minus1: ue(v)
    EXPECT_TRUE(num_slice_groups > 0);
    bit_writer.WriteExpGolomb(num_slice_groups - 1);;

    if (num_slice_groups > 1) {
        // slice_group_map_type: ue(v)
        bit_writer.WriteExpGolomb(slice_group_map_type);
        switch (slice_group_map_type) {
        case 0:
            for (int i = 0; i < num_slice_groups; ++i) {
                // run_length_minus1[iGroup]: ue(v)
                bit_writer.WriteExpGolomb(kIgnored);
            }
            break;
        case 2:
            for (int i = 0; i < num_slice_groups; ++i) {
                // top_left[iGroup]: ue(v)
                bit_writer.WriteExpGolomb(kIgnored);
                // bottom_right[iGroup]: ue(v)
                bit_writer.WriteExpGolomb(kIgnored);
            }
            break;
        case 3:
        case 4:
        case 5:
            // slice_group_change_direction_flag: u(1)
            bit_writer.WriteBits(kIgnored, 1);
            // slice_group_change_rate_minus1: ue(v)
            bit_writer.WriteExpGolomb(kIgnored);
            break;
        case 6: {
            bit_writer.WriteExpGolomb(pic_size_in_map_units - 1);

            uint32_t slice_group_id_bits = 0;
            // If num_slice_groups is not a power of two an additional bit is
            // required
            // to account for the ceil() of log2() below.
            if ((num_slice_groups & (num_slice_groups - 1)) != 0) {
                ++slice_group_id_bits;
            }
            while (num_slice_groups > 0) {
                num_slice_groups >>= 1;
                ++slice_group_id_bits;
            }

            for (int i = 0; i < pic_size_in_map_units; ++i) {
                // slice_group_id[i]: u(v)
                // Represented by ceil(log2(num_slice_groups_minus1 + 1)) bits.
                bit_writer.WriteBits(kIgnored, slice_group_id_bits);
            }
            break;
        }
        default:
            break;
        }
    }

    // num_ref_idx_l0_default_active_minus1: ue(v)
    bit_writer.WriteExpGolomb(kIgnored);
    // num_ref_idx_l1_default_active_minus1: ue(v)
    bit_writer.WriteExpGolomb(kIgnored);
    // weighted_pred_flag: u(1)
    bit_writer.WriteBits(pps.weighted_pred_flag ? 1 : 0, 1);
    // weighted_bipred_idc: u(2)
    bit_writer.WriteBits(pps.weighted_bipred_idc, 2);

    // pic_init_qp_minus26: se(v)
    bit_writer.WriteSignedExpGolomb(pps.pic_init_qp_minus26);
    // pic_init_qs_minus26: se(v)
    bit_writer.WriteExpGolomb(kIgnored);
    // chroma_qp_index_offset: se(v)
    bit_writer.WriteExpGolomb(kIgnored);
    // deblocking_filter_control_present_flag: u(1)
    // constrained_intra_pred_flag: u(1)
    bit_writer.WriteBits(kIgnored, 2);
    // redundant_pic_cnt_present_flag: u(1)
    bit_writer.WriteBits(pps.redundant_pic_cnt_present_flag, 1);

    size_t byte_offset;
    size_t bit_offset;
    bit_writer.GetCurrentOffset(&byte_offset, &bit_offset);
    if (bit_offset > 0) {
        bit_writer.WriteBits(0, 8 - bit_offset);
        bit_writer.GetCurrentOffset(&byte_offset, &bit_offset);
    }

    h264::NalUnit::WriteRbsp(data, byte_offset, out_buffer);
}

class PpsParserTest : public ::testing::Test {
public:
    PpsParserTest() {}
    ~PpsParserTest() override {}

    void RunTest() {
        VerifyParsing(generated_pps_, 0, 1, 0);
        const int kMaxSliceGroups = 17;  // Arbitrarily large.
        const int kMaxMapType = 6;
        int slice_group_bits = 0;
        for (int slice_group = 2; slice_group < kMaxSliceGroups; ++slice_group) {
            if ((slice_group & (slice_group - 1)) == 0) {
                // Slice group at a new power of two - increase slice_group_bits.
                ++slice_group_bits;
            }
            for (int map_type = 0; map_type <= kMaxMapType; ++map_type) {
                if (map_type == 1) {
                    // TODO: Implement support for dispersed slice group map type.
                    // See 8.2.2.2 Specification for dispersed slice group map type.
                    continue;
                } else if (map_type == 6) {
                    int max_pic_size = 1 << slice_group_bits;
                    for (int pic_size = 1; pic_size < max_pic_size; ++pic_size) {
                        VerifyParsing(generated_pps_, map_type, slice_group, pic_size);
                    }
                } else {
                    VerifyParsing(generated_pps_, map_type, slice_group, 0);
                }
            }
        }
    }

    void VerifyParsing(const PpsParser::PpsState& pps,
                       int slice_group_map_type,
                       int num_slice_groups,
                       int pic_size_in_map_units) {
        buffer_.clear();
        WritePps(pps, slice_group_map_type, num_slice_groups, pic_size_in_map_units, buffer_);
        parsed_pps_ = PpsParser::ParsePps(buffer_.data(), buffer_.size());
        EXPECT_TRUE(parsed_pps_.has_value());
        EXPECT_EQ(pps.bottom_field_pic_order_in_frame_present_flag,
                parsed_pps_->bottom_field_pic_order_in_frame_present_flag);
        EXPECT_EQ(pps.weighted_pred_flag, parsed_pps_->weighted_pred_flag);
        EXPECT_EQ(pps.weighted_bipred_idc, parsed_pps_->weighted_bipred_idc);
        EXPECT_EQ(pps.entropy_coding_mode_flag, parsed_pps_->entropy_coding_mode_flag);
        EXPECT_EQ(pps.redundant_pic_cnt_present_flag, parsed_pps_->redundant_pic_cnt_present_flag);
        EXPECT_EQ(pps.pic_init_qp_minus26, parsed_pps_->pic_init_qp_minus26);
        EXPECT_EQ(pps.id, parsed_pps_->id);
        EXPECT_EQ(pps.sps_id, parsed_pps_->sps_id);
    }

public:
    PpsParser::PpsState generated_pps_;

private:
    std::vector<uint8_t> buffer_;
    std::optional<PpsParser::PpsState> parsed_pps_;
};

TEST_F(PpsParserTest, ZeroPps) {
    RunTest();
}

TEST_F(PpsParserTest, MaxPps) {
    generated_pps_.bottom_field_pic_order_in_frame_present_flag = true;
    generated_pps_.pic_init_qp_minus26 = 25;
    generated_pps_.redundant_pic_cnt_present_flag = 1;  // 1 bit value.
    generated_pps_.weighted_bipred_idc = (1 << 2) - 1;  // 2 bit value.
    generated_pps_.weighted_pred_flag = true;
    generated_pps_.entropy_coding_mode_flag = true;
    generated_pps_.id = 2;
    generated_pps_.sps_id = 1;
    RunTest();

    generated_pps_.pic_init_qp_minus26 = -25;
    RunTest();
}

TEST_F(PpsParserTest, PpsIdFromSlice) {
    // 0xb7, 0x40,
    // 1011 0111 0100 0000
    // 1 - 011 - 011
    std::optional<uint32_t> pps_id = PpsParser::ParsePpsIdFromSlice(&kH264IdrSlice[1/* NAL unit header */], sizeof(kH264IdrSlice) - 1);
    ASSERT_TRUE(pps_id.has_value());
    EXPECT_EQ(2u, pps_id.value());
}

} // namespace test
} // namespace naivertc