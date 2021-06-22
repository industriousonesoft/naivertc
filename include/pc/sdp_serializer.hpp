#ifndef _PC_SDP_SERIALIZER_H_
#define _PC_SDP_SERIALIZER_H_

#include "common/defines.hpp"
#include "pc/sdp_entry.hpp"
#include "pc/sdp_defines.hpp"

#include <string>

namespace naivertc {

class RTC_CPP_EXPORT SDPSerializer {
public:
    SDPSerializer(const std::string& sdp, sdp::Type type = sdp::Type::UNSPEC, sdp::Role role = sdp::Role::ACT_PASS);
    SDPSerializer(const std::string& sdp, std::string type_string);

    sdp::Type type();
    sdp::Role role();

    void hintType(sdp::Type type);

    static sdp::Type StringToType(const std::string& type_string);
    static std::string TypeToString(sdp::Type type);

private:

    sdp::Type type_;
    sdp::Role role_;
};

}

#endif