#include "pc/sdp_serializer.hpp"
#include "common/str_utils.hpp"

#include <sstream>
#include <unordered_map>

namespace naivertc {
namespace sdp {
Serializer::Serializer(const std::string& sdp, Type type, Role role) : 
    type_(Type::UNSPEC),
    role_(role) {
    hintType(type);

    int index = -1;
}

Serializer::Serializer(const std::string& sdp, std::string type_string) : 
    Serializer(sdp, StringToType(type_string), Role::ACT_PASS) {
}

Type Serializer::type() {
    return type_;
}

Role Serializer::role() {
    return role_;
}

void Serializer::hintType(Type type) {
    if (type_ == Type::UNSPEC) {
        type_ = type;
        if (type_ == Type::ANSWER && role_ == Role::ACT_PASS) {
            // ActPass is illegal for an answer, so reset to Passive
            role_ = Role::PASSIVE;
        }
    }
}

}
} // end of naive rtc