#include "pc/configuration.hpp"

#include <plog/Log.h>

#include <regex>
#include <vector>
#include <optional>

namespace naivertc {

// eg: stun:stun.l.google.com:19302
// eg: turn:numb.viagenie.ca:3478?transport=udp&username=28224511:1379330808&credential=JZEOEt2V3Qb0y27GRntt2u2PAYA
IceServer::IceServer(const std::string& url) {
     // Parsing a URI Reference with a Regular Expression
    // Modified regex from RFC 3986, see https://tools.ietf.org/html/rfc3986#appendix-B
	static const char *rs =
	    R"(^(([^:.@/?#]+):)?(/{0,2}((([^:@]*)(:([^@]*))?)@)?(([^:/?#]*)(:([^/?#]*))?))?([^?#]*)(\?([^#]*))?(#(.*))?)";
	static const std::regex r(rs, std::regex::extended);

    std::smatch m;
    if (!std::regex_match(url, m, r) || m[10].length() == 0) {
        throw std::invalid_argument("Invalid Ice server url: " + url);
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

    host_name_ = components[10].value();
    while (!host_name_.empty() && host_name_.front() == '[')
		host_name_.erase(host_name_.begin());
	while (!host_name_.empty() && host_name_.back() == ']')
		host_name_.pop_back();

    std::string service = components[12].value_or(relay_type_ == RelayType::TURN_TLS ? "5349" : "3478");
	try {
		port_ = uint16_t(std::stoul(service));
	} catch (...) {
		throw std::invalid_argument("Invalid ICE server port in URL: " + service);
	}
}

IceServer::IceServer(std::string host_name, uint16_t port) 
    : host_name_(host_name), port_(port), type_(Type::STUN) {}

IceServer::IceServer(std::string host_name, std::string service) 
    : host_name_(host_name), type_(Type::STUN) {
    try {
		port_ = uint16_t(std::stoul(service));
	} catch (...) {
		throw std::invalid_argument("Invalid ICE server port: " + service);
	}
}

IceServer::IceServer(std::string host_name, uint16_t port, std::string username, std::string password, RelayType relay_type) 
    : host_name_(host_name), port_(port), type_(Type::TURN), username_(username), password_(password), relay_type_(relay_type) {}

IceServer::IceServer(std::string host_name, std::string service, std::string username, std::string password, RelayType relay_type) 
    : host_name_(host_name), type_(Type::TURN), username_(username), password_(password), relay_type_(relay_type) {
    try {
		port_ = uint16_t(std::stoul(service));
	} catch (...) {
		throw std::invalid_argument("Invalid ICE server port: " + service);
	}
}

}