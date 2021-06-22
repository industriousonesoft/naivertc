#ifndef _PC_SDP_SERIALIZER_H_
#define _PC_SDP_SERIALIZER_H_

#include "common/defines.hpp"
#include "pc/sdp_entry.hpp"
#include "pc/sdp_defines.hpp"

#include <string>

namespace naivertc {
namespace sdp {

class RTC_CPP_EXPORT Serializer {
public:
    Serializer(const std::string& sdp, Type type = Type::UNSPEC, Role role = Role::ACT_PASS);
    Serializer(const std::string& sdp, std::string type_string);

    Type type();
    Role role();

    void hintType(Type type);

    static Type StringToType(const std::string& type_string);
    static std::string TypeToString(Type type);

private:

    Type type_;
    Role role_;
};

}
}

#endif