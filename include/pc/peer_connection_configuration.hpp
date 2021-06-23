#ifndef _PEER_CONNECTION_CONFIGURATION_H_
#define _PEER_CONNECTION_CONFIGURATION_H_

#include "common/defines.hpp"

#include <string>

namespace naivertc {

struct RTC_CPP_EXPORT IceServer {
    enum class Type { STUN, TURN };
    enum class RelayType { TURN_UDP, TURN_TCP, TURN_TLS };

    IceServer(const std::string& url);

    // STUN
    IceServer(std::string host_name, uint16_t port);
    IceServer(std::string host_name, std::string service);

    // TURN
    IceServer(std::string host_name, uint16_t port, std::string user_name, std::string password, RelayType relay_type = RelayType::TURN_UDP);
    IceServer(std::string host_name, std::string service, std::string user_name, std::string password, RelayType relay_type = RelayType::TURN_UDP);

    std::string host_name() const { return host_name_; }
    uint16_t port() const { return port_; }
    Type type() const { return type_; }
    RelayType relay_type() const { return relay_type_; }
    std::string user_name() const { return user_name_; }
    std::string password() const { return password_; }

private:
    std::string host_name_;
    uint16_t port_;
    Type type_;
    RelayType relay_type_;
    std::string user_name_;
    std::string password_;
};

}

#endif