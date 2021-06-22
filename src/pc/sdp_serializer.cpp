#include "pc/sdp_serializer.hpp"
#include "common/str_utils.hpp"

namespace naivertc {
SDPSerializer::SDPSerializer(const std::string& sdp, Type type, Role role) : 
    type_(type),
    role_(role)
    {
}

SDPSerializer::SDPSerializer(const std::string& sdp, std::string typeStr) {

}

}