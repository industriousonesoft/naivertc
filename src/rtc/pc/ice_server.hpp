#ifndef _RTC_PC_ICE_SERVER_H_
#define _RTC_PC_ICE_SERVER_H_

#include "base/defines.hpp"

#include <string>

namespace naivertc {

// IceServer
struct IceServer {
    enum class Type { STUN, TURN };
    enum class RelayType { TURN_UDP, TURN_TCP, TURN_TLS };

    IceServer(const std::string& url_string);

    // STUN
    IceServer(const std::string hostname, uint16_t port);
    IceServer(const std::string hostname, const std::string service);

    // TURN
    IceServer(const std::string hostname, uint16_t port, const std::string username, const std::string password, RelayType relay_type = RelayType::TURN_UDP);
    IceServer(const std::string hostname, const std::string service, const std::string username, const std::string password, RelayType relay_type = RelayType::TURN_UDP);

    const std::string hostname() const { return hostname_; }
    uint16_t port() const { return port_; }
    Type type() const { return type_; }
    RelayType relay_type() const { return relay_type_; }
    const std::string username() const { return username_; }
    const std::string password() const { return password_; }

    void set_username(const std::string username) { username_ = username; }
    void set_password(const std::string password) { password_ = password; }

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

} // namespace naivertc

#endif
