#include "rtc/media/video/codecs/h264/sprop_parameter_parser.hpp"

#include <boost/beast/core/detail/base64.hpp>

namespace naivertc {
namespace h264 {

bool SpropParameterParser::Parse(const std::string& sprop) {
    size_t separator_pos = sprop.find(',');
    if (separator_pos <= 0 || separator_pos > sprop.length() - 1) {
        return false;
    }
    std::string sps_str = sprop.substr(0, separator_pos);
    // Decode SPS in base64
    sps_.resize(boost::beast::detail::base64::decoded_size(sps_str.size()));
    auto ret = boost::beast::detail::base64::decode(sps_.data(), sps_str.data(), sps_str.size());
    if (/* written bytes */ret.first <= 0 || /* read bytes */ret.second <= 0) {
        return false;
    }
    sps_.resize(ret.first);
    
    // Decode PPS in base64
    std::string pps_str = sprop.substr(separator_pos + 1, std::string::npos);
    pps_.resize(boost::beast::detail::base64::decoded_size(pps_str.size()));
    ret = boost::beast::detail::base64::decode(pps_.data(), pps_str.data(), pps_str.size());
    if (/* written bytes */ret.first <= 0 || /* read bytes */ret.second <= 0) {
        return false;
    }
    pps_.resize(ret.first);
    return true;
}

} // namespace h264
} // namespace naivertc
