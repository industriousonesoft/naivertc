#ifndef _RTC_SDP_UTILS_H_
#define _RTC_SDP_UTILS_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_entry.hpp"

#include <string>
#include <optional>

namespace naivertc {
namespace sdp {

sdp::Type ToType(const std::string_view type_string);
std::string ToString(sdp::Type type);
std::string ToString(sdp::Role role);

// format: sha-256 8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB
std::optional<std::string> ParseFingerprintAttribute(std::string_view attr_line);

bool IsSHA256Fingerprint(const std::string_view fingerprint);

} // namespace sdp
} // namespace naivertc

#endif