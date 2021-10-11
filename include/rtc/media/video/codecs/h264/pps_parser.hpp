#ifndef _RTC_MEDIA_VIDEO_CODEC_H264_PPS_PARSER_H_
#define _RTC_MEDIA_VIDEO_CODEC_H264_PPS_PARSER_H_

#include "base/defines.hpp"
#include "rtc/base/bit_io_reader.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT PpsParser {
public:
    struct PpsState {
        PpsState() = default;

        bool bottom_field_pic_order_in_frame_present_flag = false;
        bool weighted_pred_flag = false;
        bool entropy_coding_mode_flag = false;
        uint32_t weighted_bipred_idc = false;
        uint32_t redundant_pic_cnt_present_flag = 0;
        int pic_init_qp_minus26 = 0;
        uint32_t id = 0;
        uint32_t sps_id = 0;
    };
public:
    static std::optional<PpsState> ParsePps(const uint8_t* data, size_t size);

    static bool ParsePpsIds(const uint8_t* data, size_t size,  uint32_t* pps_id, uint32_t* sps_id);
    static std::optional<uint32_t> ParsePpsIdFromSlice(const uint8_t* data, size_t size);
private:
    static std::optional<PpsState> ParseInternal(BitReader& bit_reader);
    static bool ParsePpsIdsInternal(BitReader& bit_reader, uint32_t* pps_id, uint32_t* sps_id);
};
    
} // namespace naivertc


#endif