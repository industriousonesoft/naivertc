#ifndef _RTC_MEDIA_VIDEO_CODECS_H264_H_
#define _RTC_MEDIA_VIDEO_CODECS_H264_H_

#include "base/defines.hpp"
#include "rtc/base/bit_io_reader.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT SpsParser {
public:
    struct SpsState {
        SpsState();
        SpsState(const SpsState&);
        ~SpsState();

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t delta_pic_order_always_zero_flag = 0;
        uint32_t separate_colour_plane_flag = 0;
        uint32_t frame_mbs_only_flag = 0;
        uint32_t log2_max_frame_num = 4;          // Smallest valid value.
        uint32_t log2_max_pic_order_cnt_lsb = 4;  // Smallest valid value.
        uint32_t pic_order_cnt_type = 0;
        uint32_t max_num_ref_frames = 0;
        uint32_t vui_params_present = 0;
        uint32_t id = 0;
    };
public:
    // Unpack RBSP and parse SPS state from the supplied buffer.
    static std::optional<SpsState> ParseSps(const uint8_t* data, size_t length);

protected:
    // Parse the SPS state, up till the VUI part, for a bit buffer where RBSP
    // decoding has already been performed.
    static std::optional<SpsState> ParseSpsUpToVui(BitReader& bit_reader);
};
    
} // namespace naivertc


#endif