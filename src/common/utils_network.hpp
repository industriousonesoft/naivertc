#ifndef _COMMON_UTILS_NETWORK_H_
#define _COMMON_UTILS_NETWORK_H_

#include "base/defines.hpp"

#include <optional>
#include <string>

namespace naivertc {
namespace utils {
namespace network {

enum class FamilyType: int {
    UNSPEC = 0,
    IP_V4,
    IP_V6
};

enum class ProtocolType {
    UNKNOWN,
    UDP,
    TCP
};

struct RTC_CPP_EXPORT ResolveResult {
    std::string address;
    uint16_t port;
    bool is_ipv6;
};

RTC_CPP_EXPORT std::optional<ResolveResult> Resolve(std::string_view hostname, std::string_view server_port, FamilyType family_type, ProtocolType protocol_type, bool is_simple = true);
RTC_CPP_EXPORT std::optional<ResolveResult> UnspecfiedResolve(std::string_view hostname, std::string_view server_port, ProtocolType protocol_type, bool is_simple = true);
RTC_CPP_EXPORT std::optional<ResolveResult> IPv4Resolve(std::string_view hostname, std::string_view server_port, ProtocolType protocol_type, bool is_simple = true);
RTC_CPP_EXPORT std::optional<ResolveResult> IPv6Resolve(std::string_view hostname, std::string_view server_port, ProtocolType protocol_type, bool is_simple = true);

} // namespace network
} // namespace utils
} // namespace naivertc


#endif