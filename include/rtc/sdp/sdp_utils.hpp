#ifndef _RTC_SDP_UTILS_H_
#define _RTC_SDP_UTILS_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_entry.hpp"

#include <string>
#include <optional>

namespace naivertc {
namespace sdp {

RTC_CPP_EXPORT sdp::Type StringToType(const std::string& type_string);
RTC_CPP_EXPORT std::string TypeToString(sdp::Type type);
RTC_CPP_EXPORT std::string RoleToString(sdp::Role role);

// format: sha-256 8F:B5:D9:8F:53:7D:A9:B0:CE:01:3E:CB:30:BE:40:AC:33:42:25:FC:C4:FC:55:74:B9:8D:48:B0:02:5A:A8:EB
RTC_CPP_EXPORT std::optional<std::string> ParseFingerprintAttribute(std::string_view attr_line);

RTC_CPP_EXPORT bool IsSHA256Fingerprint(std::string_view fingerprint);

} // namespace sdp
} // namespace naivertc

#endif