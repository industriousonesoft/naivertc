#include "rtc/media/video/codecs/h264/sps_parser.hpp"
#include "rtc/media/video/codecs/h264/nalunit.hpp"

namespace naivertc {
namespace {
constexpr int kScalingDeltaMin = -128;
constexpr int kScaldingDeltaMax = 127;
} // namespace

SpsParser::SpsState::SpsState() = default;
SpsParser::SpsState::SpsState(const SpsState&) = default;
SpsParser::SpsState::~SpsState() = default;

// Based off the 02/2014 version of the H.264 standard.
// See http://www.itu.int/rec/T-REC-H.264
// Unpack RBSP and parse SPS state from the supplied buffer.
std::optional<SpsParser::SpsState> SpsParser::ParseSps(const uint8_t* data, size_t length) {
    std::vector<uint8_t> rbsp_buffer = h264::NalUnit::RetrieveRbspFromEbsp(data, length);
    BitReader bit_reader(rbsp_buffer.data(), rbsp_buffer.size());
    return ParseSpsUpToVui(bit_reader);
}

std::optional<SpsParser::SpsState> SpsParser::ParseSpsUpToVui(BitReader& bit_reader) {
    SpsState sps_state;

    // The golomb values we have to read, not just consume.
    uint32_t golomb_ignored = 0;

    // chroma_format_idc will be ChromaArrayType if separate_colour_plane_flag is
    // 0. It defaults to 1, when not specified.
    uint32_t chroma_format_idc = 1;

    // profile_idc: u(8). We need it to determine if we need to read/skip chroma
    // formats.
    uint8_t profile_idc;
    if (!bit_reader.ReadByte(profile_idc)) {
        return std::nullopt;
    }
    // constraint_set0_flag through constraint_set5_flag + reserved_zero_2bits
    // 1 bit each for the flags + 2 bits = 8 bits = 1 byte.
    if (!bit_reader.ConsumeBits(8)) {
        return std::nullopt;
    }
    // level_idc: u(8)
    if (!bit_reader.ConsumeBits(8)) {
        return std::nullopt;
    }
    // seq_parameter_set_id: ue(v)
    if (!bit_reader.ReadExpGolomb(sps_state.id)) {
        return std::nullopt;
    }
    sps_state.separate_colour_plane_flag = 0;
    // See if profile_idc has chroma format information.
    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
        profile_idc == 244 || profile_idc == 44 || profile_idc == 83 ||
        profile_idc == 86 || profile_idc == 118 || profile_idc == 128 ||
        profile_idc == 138 || profile_idc == 139 || profile_idc == 134) {
        // chroma_format_idc: ue(v)
        if(!bit_reader.ReadExpGolomb(chroma_format_idc)) {
            return std::nullopt;
        }
        if (chroma_format_idc == 3) {
            // separate_colour_plane_flag: u(1)
            if(!bit_reader.ReadBits(1, sps_state.separate_colour_plane_flag)) {
                return std::nullopt;
            }
        }
        // bit_depth_luma_minus8: ue(v)
        if(!bit_reader.ReadExpGolomb(golomb_ignored)) {
            return std::nullopt;
        }
        // bit_depth_chroma_minus8: ue(v)
        if(!bit_reader.ReadExpGolomb(golomb_ignored)) {
            return std::nullopt;
        }
        // qpprime_y_zero_transform_bypass_flag: u(1)
        if(!bit_reader.ConsumeBits(1)) {
            return std::nullopt;
        }
        // seq_scaling_matrix_present_flag: u(1)
        uint32_t seq_scaling_matrix_present_flag;
        if(!bit_reader.ReadBits(1, seq_scaling_matrix_present_flag)) {
            return std::nullopt;
        }
        if (seq_scaling_matrix_present_flag) {
            // Process the scaling lists just enough to be able to properly
            // skip over them, so we can still read the resolution on streams
            // where this is included.
            int scaling_list_count = (chroma_format_idc == 3 ? 12 : 8);
            for (int i = 0; i < scaling_list_count; ++i) {
                // seq_scaling_list_present_flag[i]  : u(1)
                uint32_t seq_scaling_list_present_flags;
                if(!bit_reader.ReadBits(1, seq_scaling_list_present_flags)) {
                    return std::nullopt;
                }
                if (seq_scaling_list_present_flags != 0) {
                    int last_scale = 8;
                    int next_scale = 8;
                    int size_of_scaling_list = i < 6 ? 16 : 64;
                    for (int j = 0; j < size_of_scaling_list; j++) {
                        if (next_scale != 0) {
                            int32_t delta_scale;
                            // delta_scale: se(v)
                            if(!bit_reader.ReadSignedExpGolomb(delta_scale)) {
                                return std::nullopt;
                            }
                            if(delta_scale >= kScalingDeltaMin &&
                               delta_scale <= kScaldingDeltaMax) {
                                next_scale = (last_scale + delta_scale + 256) % 256;
                            }else {
                                return std::nullopt;
                            }
                            
                        }
                        if (next_scale != 0) {
                            last_scale = next_scale;
                        }
                    }
                }
            }
        }
    }
    // log2_max_frame_num and log2_max_pic_order_cnt_lsb are used with
    // BitBuffer::ReadBits, which can read at most 32 bits at a time. We also have
    // to avoid overflow when adding 4 to the on-wire golomb value, e.g., for evil
    // input data, ReadExpGolomb might return 0xfffc.
    const uint32_t kMaxLog2Minus4 = 32 - 4;

    // log2_max_frame_num_minus4: ue(v)
    uint32_t log2_max_frame_num_minus4;
    if (!bit_reader.ReadExpGolomb(log2_max_frame_num_minus4) ||
        log2_max_frame_num_minus4 > kMaxLog2Minus4) {
        return std::nullopt;
    }
    sps_state.log2_max_frame_num = log2_max_frame_num_minus4 + 4;

    // pic_order_cnt_type: ue(v)
    if(!bit_reader.ReadExpGolomb(sps_state.pic_order_cnt_type)) {
        return std::nullopt;
    }
    if (sps_state.pic_order_cnt_type == 0) {
        // log2_max_pic_order_cnt_lsb_minus4: ue(v)
        uint32_t log2_max_pic_order_cnt_lsb_minus4;
        if (!bit_reader.ReadExpGolomb(log2_max_pic_order_cnt_lsb_minus4) ||
            log2_max_pic_order_cnt_lsb_minus4 > kMaxLog2Minus4) {
            return std::nullopt;
        }
        sps_state.log2_max_pic_order_cnt_lsb = log2_max_pic_order_cnt_lsb_minus4 + 4;
    } else if (sps_state.pic_order_cnt_type == 1) {
        // delta_pic_order_always_zero_flag: u(1)
        if(!bit_reader.ReadBits(1, sps_state.delta_pic_order_always_zero_flag)) {
            return std::nullopt;
        }
        // offset_for_non_ref_pic: se(v)
        if(!bit_reader.ReadExpGolomb(golomb_ignored)) {
            return std::nullopt;
        }
        // offset_for_top_to_bottom_field: se(v)
        if(!bit_reader.ReadExpGolomb(golomb_ignored)) {
            return std::nullopt;
        }
        // num_ref_frames_in_pic_order_cnt_cycle: ue(v)
        uint32_t num_ref_frames_in_pic_order_cnt_cycle;
        if(!bit_reader.ReadExpGolomb(num_ref_frames_in_pic_order_cnt_cycle)) {
            return std::nullopt;
        }
        for (size_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i) {
            // offset_for_ref_frame[i]: se(v)
            if(!bit_reader.ReadExpGolomb(golomb_ignored)) {
                return std::nullopt;
            }
        }
    }
    // max_num_ref_frames: ue(v)
    if(!bit_reader.ReadExpGolomb(sps_state.max_num_ref_frames)) {
        return std::nullopt;
    }
    // gaps_in_frame_num_value_allowed_flag: u(1)
    if(!bit_reader.ConsumeBits(1)) {
        return std::nullopt;
    }
    //
    // IMPORTANT ONES! Now we're getting to resolution. First we read the pic
    // width/height in macroblocks (16x16), which gives us the base resolution,
    // and then we continue on until we hit the frame crop offsets, which are used
    // to signify resolutions that aren't multiples of 16.
    //
    // pic_width_in_mbs_minus1: ue(v)
    uint32_t pic_width_in_mbs_minus1;
    if(!bit_reader.ReadExpGolomb(pic_width_in_mbs_minus1)) {
        return std::nullopt;
    }
    // pic_height_in_map_units_minus1: ue(v)
    uint32_t pic_height_in_map_units_minus1;
    if(!bit_reader.ReadExpGolomb(pic_height_in_map_units_minus1)) {
        return std::nullopt;
    }
    // frame_mbs_only_flag: u(1)
    if(!bit_reader.ReadBits(1, sps_state.frame_mbs_only_flag)) {
        return std::nullopt;
    }
    if (!sps_state.frame_mbs_only_flag) {
        // mb_adaptive_frame_field_flag: u(1)
        if(!bit_reader.ConsumeBits(1)) {
            return std::nullopt;
        }
    }
    // direct_8x8_inference_flag: u(1)
    if(!bit_reader.ConsumeBits(1)) {
        return std::nullopt;
    }
    //
    // MORE IMPORTANT ONES! Now we're at the frame crop information.
    //
    // frame_cropping_flag: u(1)
    uint32_t frame_cropping_flag;
    uint32_t frame_crop_left_offset = 0;
    uint32_t frame_crop_right_offset = 0;
    uint32_t frame_crop_top_offset = 0;
    uint32_t frame_crop_bottom_offset = 0;
    if(!bit_reader.ReadBits(1, frame_cropping_flag)) {
        return std::nullopt;
    }
    if (frame_cropping_flag) {
        // frame_crop_{left, right, top, bottom}_offset: ue(v)
        if(!bit_reader.ReadExpGolomb(frame_crop_left_offset)) {
            return std::nullopt;
        }
        if(!bit_reader.ReadExpGolomb(frame_crop_right_offset)) {
            return std::nullopt;
        }
        if(!bit_reader.ReadExpGolomb(frame_crop_top_offset)) {
            return std::nullopt;
        }
        if(!bit_reader.ReadExpGolomb(frame_crop_bottom_offset)) {
            return std::nullopt;
        }
    }
    // vui_parameters_present_flag: u(1)
    if(!bit_reader.ReadBits(1, sps_state.vui_params_present)) {
        return std::nullopt;
    }

    // Far enough! We don't use the rest of the SPS.

    // Start with the resolution determined by the pic_width/pic_height fields.
    sps_state.width = 16 * (pic_width_in_mbs_minus1 + 1);
    sps_state.height = 16 * (2 - sps_state.frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1);

    // Figure out the crop units in pixels. That's based on the chroma format's
    // sampling, which is indicated by chroma_format_idc.
    if (sps_state.separate_colour_plane_flag || chroma_format_idc == 0) {
        frame_crop_bottom_offset *= (2 - sps_state.frame_mbs_only_flag);
        frame_crop_top_offset *= (2 - sps_state.frame_mbs_only_flag);
    } else if (!sps_state.separate_colour_plane_flag && chroma_format_idc > 0) {
        // Width multipliers for formats 1 (4:2:0) and 2 (4:2:2).
        if (chroma_format_idc == 1 || chroma_format_idc == 2) {
            frame_crop_left_offset *= 2;
            frame_crop_right_offset *= 2;
        }
        // Height multipliers for format 1 (4:2:0).
        if (chroma_format_idc == 1) {
            frame_crop_top_offset *= 2;
            frame_crop_bottom_offset *= 2;
        }
    }
    // Subtract the crop for each dimension.
    sps_state.width -= (frame_crop_left_offset + frame_crop_right_offset);
    sps_state.height -= (frame_crop_top_offset + frame_crop_bottom_offset);

    return sps_state;
}
    
} // namespace naivertc
