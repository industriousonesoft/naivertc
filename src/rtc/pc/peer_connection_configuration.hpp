#ifndef _RTC_PEER_CONNECTION_CONFIGURATION_H_
#define _RTC_PEER_CONNECTION_CONFIGURATION_H_

#include "base/defines.hpp"
#include "rtc/pc/ice_server.hpp"

#include <string>
#include <vector>
#include <optional>

namespace naivertc {

// CertificateType
enum class CertificateType {
    DEFAULT,
    ECDSA,
    RSA
};

#if USE_NICE

struct RTC_CPP_EXPORT ProxyServer {
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
struct RTC_CPP_EXPORT RtcConfiguration {
    // Ice settings
    std::vector<IceServer> ice_servers;
    
#if USE_NICE
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
    uint16_t port_range_begin = 1024;
    uint16_t port_range_end = 65535;

    // MTU: Maximum Transmission Unit
    std::optional<size_t> mtu;

    // SCTP
    std::optional<uint16_t> local_sctp_port;
    std::optional<size_t> sctp_max_message_size;

};

}

#endif