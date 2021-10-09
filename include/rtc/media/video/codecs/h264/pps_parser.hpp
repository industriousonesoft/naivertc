#ifndef _RTC_MEDIA_VIDEO_CODEC_H264_PPS_PARSER_H_
#define _RTC_MEDIA_VIDEO_CODEC_H264_PPS_PARSER_H_

#include "base/defines.hpp"
#include "rtc/base/bit_reader.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT PpsParser {
public:
    static bool ParsePpsIds(const uint8_t* data, size_t size,  uint32_t* pps_id, uint32_t* sps_id);
    static std::optional<uint32_t> ParsePpsIdsFromSlice(const uint8_t* data, size_t size);
private:
    static bool ParsePpsIdsInternal(BitReader& bit_reader, uint32_t* pps_id, uint32_t* sps_id);
};
    
} // namespace naivertc


#endif