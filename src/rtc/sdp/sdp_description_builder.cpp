#include "rtc/sdp/sdp_description.hpp"

namespace naivertc {
namespace sdp {

Description::Builder::Builder(Type type) 
    : type_(type) {

}

Description::Builder::~Builder() {

}

Description::Builder& Description::Builder::set_role(Role role) {
    role_ = role;
    return *this;
}

Description::Builder& Description::Builder::set_ice_ufrag(std::optional<std::string> ice_ufrag) {
    ice_ufrag_ = ice_ufrag;
    return *this;
}

Description::Builder& Description::Builder::set_ice_pwd(std::optional<std::string> ice_pwd) {
    ice_pwd_ = ice_pwd;
    return *this;
}

Description::Builder& Description::Builder::set_fingerprint(std::optional<std::string> fingerprint) {
    fingerprint_ = fingerprint;
    return *this;
}

Description Description::Builder::Build() {
    return Description(type_, role_, ice_ufrag_, ice_pwd_, fingerprint_);
}

    
} // namespace sdp
} // namespace naivertc
