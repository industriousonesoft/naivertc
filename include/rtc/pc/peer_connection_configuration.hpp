#ifndef _RTC_PEER_CONNECTION_CONFIGURATION_H_
#define _RTC_PEER_CONNECTION_CONFIGURATION_H_

#include "base/defines.hpp"

#include <string>
#include <vector>
#include <optional>

namespace naivertc {

// IceServer
struct RTC_CPP_EXPORT IceServer {
    enum class Type { STUN, TURN };
    enum class RelayType { TURN_UDP, TURN_TCP, TURN_TLS };

    IceServer(const std::string& url);

    // STUN
    IceServer(std::string hostname, uint16_t port);
    IceServer(std::string hostname, std::string service);

    // TURN
    IceServer(std::string hostname, uint16_t port, std::string username, std::string password, RelayType relay_type = RelayType::TURN_UDP);
    IceServer(std::string hostname, std::string service, std::string username, std::string password, RelayType relay_type = RelayType::TURN_UDP);

    std::string hostname() const { return hostname_; }
    uint16_t port() const { return port_; }
    Type type() const { return type_; }
    RelayType relay_type() const { return relay_type_; }
    std::string username() const { return username_; }
    std::string password() const { return password_; }

    void set_username(std::string username) { username_ = username; }
    void set_password(std::string password) { password_ = password; }

    operator std::string() const;

private:
    std::string type_to_string() const;
    std::string relay_type_to_string() const;

private:
    std::string hostname_;
    uint16_t port_;
    Type type_;
    std::string username_;
    std::string password_;
    RelayType relay_type_;
};

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

    Type type;
    std::string hostname;
    uint16_t port;
    std::string username;
    std::string password;

    ProxyServer(Type type, std::string hostname, uint16_t port, std::string username = "", std::string password = "");
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
    bool auto_negotiation = true;

    // Port range
    uint16_t port_range_begin = 1024;
    uint16_t port_range_end = 65535;

    // MTU: Maximum Transmission Unit
    std::optional<size_t> mtu;

    // Local max message size at reception
    std::optional<size_t> max_message_size;

};

}

#endif