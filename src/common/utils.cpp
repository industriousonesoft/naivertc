#include "common/utils.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

const size_t MAX_NUMERICNODE_LEN = 48; // Max IPv6 string representation length
const size_t MAX_NUMERICSERV_LEN = 6;  // Max port string representation length

namespace naivertc {
namespace utils {

// numberic
namespace numeric {
}

// string
namespace string {

bool match_prefix(const std::string_view str, const std::string_view prefix) {
    return str.size() >= prefix.size() && std::mismatch(prefix.begin(), prefix.end(), str.begin()).first == prefix.end();
}

void trim_begin(std::string &str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](char c){
        return !std::isspace(c);
    }));
}

void trim_end(std::string &str) {
    // reverse_iterator.base() -> iterator
    str.erase(std::find_if(str.rbegin(), str.rend(), [](char c){
        return !std::isspace(c);
    }).base(), str.end());
}

std::pair<std::string_view, std::string_view> parse_pair(std::string_view attr) {
    std::string_view key, value;
    if (size_t separator = attr.find(':'); separator != std::string::npos) {
        key = attr.substr(0, separator);
        value = attr.substr(separator+1);
    }else {
        key = attr;
    }
    return std::make_pair(std::move(key), std::move(value));
}

} // enamespace string

// Random
namespace random {} // namespace random

// Network
namespace network {

std::optional<ResolveResult> UnspecfiedResolve(std::string hostname, std::string server_port, ProtocolType protocol_type, bool is_simple) {
    return Resolve(std::move(hostname), std::move(server_port), FamilyType::UNSPEC, protocol_type, is_simple);
}

std::optional<ResolveResult> IPv4Resolve(std::string hostname, std::string server_port, ProtocolType protocol_type, bool is_simple) {
    return Resolve(std::move(hostname), std::move(server_port), FamilyType::IP_V4, protocol_type, is_simple);
}

std::optional<ResolveResult> IPv6Resolve(std::string hostname, std::string server_port, ProtocolType protocol_type, bool is_simple) {
    return Resolve(std::move(hostname), std::move(server_port), FamilyType::IP_V6, protocol_type, is_simple);
}

std::optional<ResolveResult> Resolve(std::string hostname, std::string server_port, FamilyType family_type, ProtocolType protocol_type, bool is_simple) {
    
    if (hostname.empty()) {
        throw std::invalid_argument("Hostname is not supposed to be empty");
    }

    struct addrinfo hints = {};
    // Family
    if (family_type == FamilyType::IP_V4) {
        hints.ai_family = AF_INET;
    }else if (family_type == FamilyType::IP_V6) {
        hints.ai_family = AF_INET6;
    }else {
        hints.ai_family = AF_UNSPEC;
    }
    
    // Protocol
    if (protocol_type == ProtocolType::UDP) {
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
    }else if (protocol_type == ProtocolType::TCP) {
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
    }

    hints.ai_flags = AI_ADDRCONFIG;

    // Host representd as numeric
    if (is_simple) {
        hints.ai_flags |= AI_NUMERICHOST;
    }

    struct addrinfo *result = nullptr;
    if (getaddrinfo(hostname.c_str(), server_port.c_str(), &hints, &result) == 0) {
        ResolveResult resolve_result;
        bool resovled = false;
        for (auto p = result; p; p = p ->ai_next) {
            char nodebuffer[MAX_NUMERICNODE_LEN];
            char servbuffer[MAX_NUMERICSERV_LEN];
            if (getnameinfo(p->ai_addr, socklen_t(p->ai_addrlen), nodebuffer,
				                MAX_NUMERICNODE_LEN, servbuffer, MAX_NUMERICSERV_LEN,
				                NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
                resolve_result.port = uint16_t(std::stoul(servbuffer));
                resolve_result.address = std::string(nodebuffer);
                resolve_result.is_ipv6 = p->ai_family == AF_INET6 ? true : false;
                resovled = true;
                break;
            }
        }
        freeaddrinfo(result);
       
        if (resovled) {
            return std::make_optional<ResolveResult>(std::move(resolve_result));
        }
    }
    return std::nullopt;
}

} // namespace network

}
}