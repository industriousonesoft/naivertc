#include "rtc/media/video/codecs/h264/pps_parser.hpp"
#include "rtc/media/video/codecs/h264/nalunit.hpp"

namespace naivertc {
namespace h264 {
namespace {
const int kMaxPicInitQpDeltaValue = 25;
const int kMinPicInitQpDeltaValue = -26;
}  // namespace

std::optional<PpsParser::PpsState> PpsParser::ParsePps(const uint8_t* data, size_t size) {
    std::vector<uint8_t> rbsp_buffer = h264::NalUnit::RetrieveRbspFromEbsp(data, size);
    BitReader bit_reader(rbsp_buffer.data(), rbsp_buffer.size());
    return ParseInternal(bit_reader);
}

bool PpsParser::ParsePpsIds(const uint8_t* data, size_t size,  uint32_t* pps_id, uint32_t* sps_id) {
    std::vector<uint8_t> rbsp_buffer = h264::NalUnit::RetrieveRbspFromEbsp(data, size);
    BitReader bit_reader(rbsp_buffer.data(), rbsp_buffer.size());
    return ParsePpsIdsInternal(bit_reader, pps_id, sps_id);
}

std::optional<uint32_t> PpsParser::ParsePpsIdFromSlice(const uint8_t* data, size_t size) {
    std::vector<uint8_t> rbsp_buffer = h264::NalUnit::RetrieveRbspFromEbsp(data, size);
    BitReader slice_reader(rbsp_buffer.data(), rbsp_buffer.size());
    uint32_t golomb_tmp;
    // first_mb_in_slice: ue(v)
    if (!slice_reader.ReadExpGolomb(golomb_tmp))
        return std::nullopt;
    // slice_type: ue(v)
    if (!slice_reader.ReadExpGolomb(golomb_tmp))
        return std::nullopt;
    // pic_parameter_set_id: ue(v)
    uint32_t slice_pps_id;
    if (!slice_reader.ReadExpGolomb(slice_pps_id))
        return std::nullopt;
    return slice_pps_id;
}

// Private methods
bool PpsParser::ParsePpsIdsInternal(BitReader& bit_reader, uint32_t* pps_id, uint32_t* sps_id) {
    if (pps_id == nullptr || !bit_reader.ReadExpGolomb(*pps_id)) {
        return false;
    }
    if (sps_id == nullptr || !bit_reader.ReadExpGolomb(*sps_id)) {
        return false;
    }
    return true;
}

std::optional<PpsParser::PpsState> PpsParser::ParseInternal(BitReader& bit_reader) {
    PpsState pps;
    if (!ParsePpsIdsInternal(bit_reader, &pps.id, &pps.sps_id)) {
        return std::nullopt;
    }
    uint32_t bits_tmp;
    uint32_t golomb_ignored;
    // entropy_coding_mode_flag: u(1)
    uint32_t entropy_coding_mode_flag;
    if (!bit_reader.ReadBits(1, entropy_coding_mode_flag)) {
        return std::nullopt;
    }
    pps.entropy_coding_mode_flag = entropy_coding_mode_flag != 0;
    // bottom_field_pic_order_in_frame_present_flag: u(1)
    uint32_t bottom_field_pic_order_in_frame_present_flag;
    if (!bit_reader.ReadBits(1, bottom_field_pic_order_in_frame_present_flag)) {
        return std::nullopt;
    }
    pps.bottom_field_pic_order_in_frame_present_flag = bottom_field_pic_order_in_frame_present_flag != 0;

    // num_slice_groups_minus1: ue(v)
    uint32_t num_slice_groups_minus1;
    if (!bit_reader.ReadExpGolomb(num_slice_groups_minus1)) {
        return std::nullopt;
    }

    if (num_slice_groups_minus1 > 0) {
        uint32_t slice_group_map_type;
        // slice_group_map_type: ue(v)
        if (!bit_reader.ReadExpGolomb(slice_group_map_type)) {
            return std::nullopt;
        }
        if (slice_group_map_type == 0) {
            for (uint32_t i_group = 0; i_group <= num_slice_groups_minus1; ++i_group) {
                // run_length_minus1[iGroup]: ue(v)
                if (!bit_reader.ReadExpGolomb(golomb_ignored)) {
                    return std::nullopt;
                }
            }
        } else if (slice_group_map_type == 1) {
            // TODO: Implement support for dispersed slice group map type.
            // See 8.2.2.2 Specification for dispersed slice group map type.
        } else if (slice_group_map_type == 2) {
            for (uint32_t i_group = 0; i_group <= num_slice_groups_minus1;
                ++i_group) {
                // top_left[iGroup]: ue(v)
                if (!bit_reader.ReadExpGolomb(golomb_ignored)) {
                    return std::nullopt;
                }
                // bottom_right[iGroup]: ue(v)
                if (!bit_reader.ReadExpGolomb(golomb_ignored)) {
                    return std::nullopt;
                }
            }
        } else if (slice_group_map_type == 3 || slice_group_map_type == 4 ||
                slice_group_map_type == 5) {
            // slice_group_change_direction_flag: u(1)
            if (!bit_reader.ReadBits(1, bits_tmp)) {
                return std::nullopt;
            }
            // slice_group_change_rate_minus1: ue(v)
            if (!bit_reader.ReadExpGolomb(golomb_ignored)) {
                return std::nullopt;
            }
        } else if (slice_group_map_type == 6) {
            // pic_size_in_map_units_minus1: ue(v)
            uint32_t pic_size_in_map_units_minus1;
            if (!bit_reader.ReadExpGolomb(pic_size_in_map_units_minus1)) {
                return std::nullopt;
            }
            uint32_t slice_group_id_bits = 0;
            uint32_t num_slice_groups = num_slice_groups_minus1 + 1;
            // If num_slice_groups is not a power of two an additional bit is required
            // to account for the ceil() of log2() below.
            if ((num_slice_groups & (num_slice_groups - 1)) != 0)
                ++slice_group_id_bits;
            while (num_slice_groups > 0) {
                num_slice_groups >>= 1;
                ++slice_group_id_bits;
            }
            for (uint32_t i = 0; i <= pic_size_in_map_units_minus1; i++) {
                // slice_group_id[i]: u(v)
                // Represented by ceil(log2(num_slice_groups_minus1 + 1)) bits.
                if (!bit_reader.ReadBits(slice_group_id_bits, bits_tmp)) {
                    return std::nullopt;
                }
            }
        }
    }
    
    // num_ref_idx_l0_default_active_minus1: ue(v)
    if (!bit_reader.ReadExpGolomb(golomb_ignored)) {
        return std::nullopt;
    }
    
    // num_ref_idx_l1_default_active_minus1: ue(v)
    if (!bit_reader.ReadExpGolomb(golomb_ignored)) {
        return std::nullopt;
    }
    
    // weighted_pred_flag: u(1)
    uint32_t weighted_pred_flag;
    if (!bit_reader.ReadBits(1, weighted_pred_flag)) {
        return std::nullopt;
    }
    
    pps.weighted_pred_flag = weighted_pred_flag != 0;
    // weighted_bipred_idc: u(2)
    if (!bit_reader.ReadBits(2, pps.weighted_bipred_idc)) {
        return std::nullopt;
    }
    
    // pic_init_qp_minus26: se(v)
    if (!bit_reader.ReadSignedExpGolomb(pps.pic_init_qp_minus26)) {
        return std::nullopt;
    }
    
    // Sanity-check parsed value
    if (pps.pic_init_qp_minus26 > kMaxPicInitQpDeltaValue ||
        pps.pic_init_qp_minus26 < kMinPicInitQpDeltaValue) {
        return std::nullopt;
    }
    
    // pic_init_qs_minus26: se(v)
    if (!bit_reader.ReadExpGolomb(golomb_ignored)) {
        return std::nullopt;
    }
    
    // chroma_qp_index_offset: se(v)
    if (!bit_reader.ReadExpGolomb(golomb_ignored)) {
        return std::nullopt;
    }
    // deblocking_filter_control_present_flag: u(1)
    // constrained_intra_pred_flag: u(1)
    if (!bit_reader.ReadBits(2, bits_tmp)) {
        return std::nullopt;
    }
    
    // redundant_pic_cnt_present_flag: u(1)
    if (!bit_reader.ReadBits(1, pps.redundant_pic_cnt_present_flag)) {
        return std::nullopt;
    }
    
    return pps;
}
    
} // namespace h264
} // namespace naivertc
