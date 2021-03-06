#include "rtc/transports/ice_transport.hpp"
#include "common/utils_string.hpp"

#include <sstream>

namespace naivertc {

IceTransport::Description::Description(
    sdp::Type type, 
    sdp::Role role, 
    std::optional<std::string> ice_ufrag, 
    std::optional<std::string> ice_pwd) 
    : type_(type),
    role_(role),
    ice_ufrag_(ice_ufrag),
    ice_pwd_(ice_pwd) {}

IceTransport::Description::~Description() {}

sdp::Type IceTransport::Description::type() const {
    return type_;
}

sdp::Role IceTransport::Description::role() const {
    return role_;
}

std::optional<std::string> IceTransport::Description::ice_ufrag() const {
    return ice_ufrag_;
}

std::optional<std::string> IceTransport::Description::ice_pwd() const {
    return ice_pwd_;
}

/* SDP generated by libnice
m=application 0 ICE/SDP
c=IN IP4 0.0.0.0
a=ice-ufrag:5gAx
a=ice-pwd:UaOtA7vsDocYINrXSTPWph
*/
std::string IceTransport::Description::GenerateSDP(const std::string eol) const {
    std::ostringstream oss;
    oss << "m=application 0 ICE/SDP" << eol;
    oss << "c=IN IP4 0.0.0.0" << eol;

    if (ice_ufrag_.has_value() && ice_pwd_.has_value()) {
        oss << "a=ice-ufrag:" << ice_ufrag_.value() << eol;
        oss << "a=ice-pwd:" << ice_pwd_.value() << eol;
    }
    
    return oss.str();
}

IceTransport::Description IceTransport::Description::Parse(std::string sdp, sdp::Type type, sdp::Role role) {
    std::istringstream iss(sdp);
    std::optional<std::string> ice_ufrag;
    std::optional<std::string> ice_pwd;
    while(iss) {
        std::string line;
        std::getline(iss, line);
        utils::string::trim_begin(line);
        utils::string::trim_end(line);
        if (line.empty()) {
            continue;
        }
        if (utils::string::match_prefix(line, "a")) {
            std::string attr = line.substr(2);
            auto [key, value] = utils::string::parse_pair(attr);

            if (key == "ice-ufrag") {
                ice_ufrag.emplace(std::move(value));
            } else if (key == "ice-pwd") {
                ice_pwd.emplace(std::move(value));
            }
            if (ice_ufrag.has_value() && ice_pwd.has_value()) {
                break;
            }
        }
    }
    return Description(type, role, ice_ufrag, ice_pwd);
}
    
} // namespace naivertc
