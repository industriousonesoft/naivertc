#include "rtc/pc/peer_connection_configuration.hpp"

#include <plog/Log.h>

#include <regex>
#include <vector>
#include <optional>
#include <sstream>

namespace naivertc {

// eg: stun:stun.l.google.com:19302
// eg: turn:numb.viagenie.ca:3478?transport=udp&username=28224511:1379330808&credential=JZEOEt2V3Qb0y27GRntt2u2PAYA
IceServer::IceServer(const std::string& url_string) {
    // Parsing a URI Reference with a Regular Expression
    // Modified regex from RFC 3986, see https://tools.ietf.org/html/rfc3986#appendix-B
    // TODO(cwp): 这段正则表达式怎么理解?
	static const char *rs =
	    R"(^(([^:.@/?#]+):)?(/{0,2}((([^:@]*)(:([^@]*))?)@)?(([^:/?#]*)(:([^/?#]*))?))?([^?#]*)(\?([^#]*))?(#(.*))?)";
	static const std::regex r(rs, std::regex::extended);

    std::smatch m;
    if (!std::regex_match(url_string, m, r) || m[10].length() == 0) {
        throw std::invalid_argument("Invalid Ice server url: " + url_string);
    }

    std::vector<std::optional<std::string>> components(m.size());
    std::transform(m.begin(), m.end(), components.begin(), [](const auto &component) {
        return component.length() > 0 ? std::make_optional(std::string(component)) : std::nullopt;
    });

    std::string scheme = components[2].value_or("stun");
    relay_type_ = RelayType::TURN_UDP;
    if (scheme == "stun" || scheme == "STUN") {
        type_ = Type::STUN;
    }else if (scheme == "turn" || scheme == "TURN") {
        type_ = Type::TURN;
    }else if (scheme == "turns" || scheme == "TURNS") {
        type_ = Type::TURN;
        relay_type_ = RelayType::TURN_TLS;
    }else {
        throw std::invalid_argument("Unknown Ice Server protocol: " + scheme);
    }

    if (auto &query = components[15]) {
        if (query->find("transport=udp") != std::string::npos) {
            relay_type_ = RelayType::TURN_UDP;
        }else if (query->find("transport=tcp") != std::string::npos) {
            relay_type_ = RelayType::TURN_TCP;
        }else if (query->find("transport=tls") != std::string::npos) {
            relay_type_ = RelayType::TURN_TLS;
        }
    }

    username_ = components[6].value_or("");
    password_ = components[8].value_or("");

    hostname_ = components[10].value();
    while (!hostname_.empty() && hostname_.front() == '[')
		hostname_.erase(hostname_.begin());
	while (!hostname_.empty() && hostname_.back() == ']')
		hostname_.pop_back();

    std::string service = components[12].value_or(relay_type_ == RelayType::TURN_TLS ? "5349" : "3478");
	try {
		port_ = uint16_t(std::stoul(service));
	} catch (...) {
		throw std::invalid_argument("Invalid ICE server port in URL: " + service);
	}
}

IceServer::IceServer(const std::string hostname, uint16_t port) 
    : hostname_(hostname), port_(port), type_(Type::STUN) {}

IceServer::IceServer(const std::string hostname, const std::string service) 
    : hostname_(std::move(hostname)), type_(Type::STUN) {
    try {
		port_ = uint16_t(std::stoul(service));
	} catch (...) {
		throw std::invalid_argument("Invalid ICE server port: " + service);
	}
}

IceServer::IceServer(const std::string hostname, uint16_t port, const std::string username, const std::string password, RelayType relay_type) 
    : hostname_(std::move(hostname)), port_(port), type_(Type::TURN), username_(std::move(username)), password_(std::move(password)), relay_type_(relay_type) {}

IceServer::IceServer(const std::string hostname, const std::string service, const std::string username, const std::string password, RelayType relay_type) 
    : hostname_(std::move(hostname)), type_(Type::TURN), username_(std::move(username)), password_(std::move(password)), relay_type_(relay_type) {
    try {
		port_ = uint16_t(std::stoul(service));
	} catch (...) {
		throw std::invalid_argument("Invalid ICE server port: " + service);
	}
}

IceServer::operator std::string() const {
    std::ostringstream desc;
    desc << "hostname: " << hostname_ << " port: " << port_ << " type: " << type_to_string();
    if (type_ == Type::TURN) {
        desc << " username: " << username_ << " password: " << password_ << " relayType: " << relay_type_to_string();
    }
    return desc.str();
}

std::string IceServer::type_to_string() const {
    switch (type_)
    {
    case Type::STUN:
        return "STUN";
    case Type::TURN:
        return "TRUN";
    default:
        return "";
    }
}

std::string IceServer::relay_type_to_string() const {
    switch (relay_type_)
    {
    case RelayType::TURN_UDP:
        return "TURN_UDP";
    case RelayType::TURN_TCP:
        return "TURN_TCP";
    case RelayType::TURN_TLS:
        return "TURN_TLS";
    default:
        return "";
    }
}

// ProxyServer
ProxyServer::ProxyServer(Type type_, const std::string hostname_, uint16_t port_, const std::string username_, const std::string password_) 
    : type(type_),
    hostname(std::move(hostname_)),
    port(port_),
    username(std::move(username_)),
    password(std::move(password_)) {}

}