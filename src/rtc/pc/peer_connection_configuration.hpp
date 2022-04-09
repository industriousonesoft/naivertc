#ifndef _RTC_PEER_CONNECTION_CONFIGURATION_H_
#define _RTC_PEER_CONNECTION_CONFIGURATION_H_

#include "base/defines.hpp"
#include "rtc/pc/ice_server.hpp"

#include <string>
#include <vector>
#include <optional>

namespace naivertc {

constexpr uint16_t kDefaultPortLowerBound = 1024;
constexpr uint16_t kDefaultPortUpperBound = 65535;

// CertificateType
enum class CertificateType {
    DEFAULT,
    ECDSA,
    RSA
};

#if defined(USE_NICE)

struct ProxyServer {
    enum class Type { 
        NONE = 0,
        SOCKS5,
        HTTP,
        LAST = HTTP
    };

    ProxyServer(Type type, 
                std::string hostname, 
                uint16_t port, 
                std::string username, 
                std::string password)
    : type(type),
      hostname(std::move(hostname)),
      port(port),
      username(std::move(username)),
      password(std::move(password)) {}

    Type type;
    std::string hostname;
    uint16_t port;
    std::string username;
    std::string password;
};

#endif

// Rtc Configuration
struct RtcConfiguration {
    // Ice settings
    std::vector<IceServer> ice_servers;
    
#if defined(USE_NICE)
    // libnice only
    std::optional<ProxyServer> proxy_server;
#else
    // libjuice only
    std::optional<std::string> bind_addresses;
#endif

    // Options
    CertificateType certificate_type = CertificateType::DEFAULT;
    bool enable_ice_tcp = false;
    bool auto_negotiation = false;

    // Port range
    uint16_t port_range_begin = kDefaultPortLowerBound;
    uint16_t port_range_end = kDefaultPortUpperBound;

    // MTU: Maximum Transmission Unit
    std::optional<size_t> mtu;

    // SCTP
    std::optional<uint16_t> local_sctp_port;
    std::optional<size_t> sctp_max_message_size;

    // Supported by default after M71
    // see: https://www.cnblogs.com/wangyiyunxin/p/14689496.html
    bool extmap_allow_mixed = true;

};

} // namespace naivertc

#endif