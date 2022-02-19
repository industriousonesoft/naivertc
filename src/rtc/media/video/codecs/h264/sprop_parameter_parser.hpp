#ifndef _RTC_MEDIA_VIDEO_CODECS_H264_SPROP_PARAMETER_PARSER_H_
#define _RTC_MEDIA_VIDEO_CODECS_H264_SPROP_PARAMETER_PARSER_H_

#include "base/defines.hpp"

#include <vector>
#include <string>

namespace naivertc {
namespace h264 {

class SpropParameterParser final {
public:
    SpropParameterParser() = default;
    ~SpropParameterParser() = default;

    bool Parse(const std::string& sprop);
    const std::vector<uint8_t>& sps_nalu() { return sps_; }
    const std::vector<uint8_t>& pps_nalu() { return pps_; }

private:
    std::vector<uint8_t> sps_;
    std::vector<uint8_t> pps_;
    DISALLOW_COPY_AND_ASSIGN(SpropParameterParser);
};
    
} // namespace h264
} // namespace naivertc


#endif