#ifndef _PC_SDP_SERIALIZER_H_
#define _PC_SDP_SERIALIZER_H_

#include "common/defines.hpp"

#include <string>

namespace naivertc {

class RTC_CPP_EXPORT SDPSerializer {
public:
    enum class Type {
        UNSPEC,
        OFFER,
        ANSWER,
        PRANSWER, // provisional answer
        ROLL_BACK
    };

    enum class Role {
        ACT_PASS,
        PASSIVE,
        ACTIVE
    };

    enum class Direction {
        SEND_ONLY,
        RECV_ONLY,
        SEND_RECV,
        INACTIVE,
        UNKNOWN
    };

    SDPSerializer(const std::string& sdp, Type type = Type::UNSPEC, Role role = Role::ACT_PASS);
    SDPSerializer(const std::string& sdp, std::string typeStr);

private:

    Type type_;
    Role role_;
};

}

#endif