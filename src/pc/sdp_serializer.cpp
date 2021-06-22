#include "pc/sdp_serializer.hpp"
#include "common/str_utils.hpp"

#include <sstream>
#include <unordered_map>

namespace naivertc {
SDPSerializer::SDPSerializer(const std::string& sdp, sdp::Type type, sdp::Role role) : 
    type_(sdp::Type::UNSPEC),
    role_(role) {
    hintType(type);

    int index = -1;
}

SDPSerializer::SDPSerializer(const std::string& sdp, std::string type_string) : 
    SDPSerializer(sdp, StringToType(type_string), sdp::Role::ACT_PASS) {
}

sdp::Type SDPSerializer::type() {
    return type_;
}

sdp::Role SDPSerializer::role() {
    return role_;
}

void SDPSerializer::hintType(sdp::Type type) {
    if (type_ == sdp::Type::UNSPEC) {
        type_ = type;
        if (type_ == sdp::Type::ANSWER && role_ == sdp::Role::ACT_PASS) {
            // ActPass is illegal for an answer, so reset to Passive
            role_ = sdp::Role::PASSIVE;
        }
    }
}

} // end of naive rtc