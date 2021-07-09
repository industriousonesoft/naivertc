#ifndef _PEER_CONNECTION_CONFIGURATION_H_
#define _PEER_CONNECTION_CONFIGURATION_H_

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
    IceServer(std::string host_name, uint16_t port);
    IceServer(std::string host_name, std::string service);

    // TURN
    IceServer(std::string host_name, uint16_t port, std::string username, std::string password, RelayType relay_type = RelayType::TURN_UDP);
    IceServer(std::string host_name, std::string service, std::string username, std::string password, RelayType relay_type = RelayType::TURN_UDP);

    std::string host_name() const { return host_name_; }
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
    std::string host_name_;
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

// DataChannelInit
struct RTC_CPP_EXPORT DataChannelInit {
    std::optional<StreamId> stream_id;
    std::string label;
    std::string protocol;
};

// Configuration
struct RTC_CPP_EXPORT Configuration {
    // Ice settings
    std::vector<IceServer> ice_servers;
    std::optional<std::string> bind_addresses;

    // Options
    CertificateType certificate_type = CertificateType::DEFAULT;
    bool enable_ice_tcp = false;
    bool auto_negotiation = false;

    // Port range
    uint16_t port_range_begin_;
    uint16_t port_range_end_;

    // MTU: Maximum Transmission Unit
    std::optional<size_t> mtu_;

    // Local max message size at reception
    std::optional<size_t> max_message_size_;

};

}

#endif