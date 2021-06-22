#include "pc/sdp_defines.hpp"

namespace naivertc {
namespace sdp {

sdp::Type StringToType(const std::string& type_string) {
    using type_map_t = std::unordered_map<std::string, sdp::Type>;
    static const type_map_t type_map = {
        {"unspec", sdp::Type::UNSPEC},
        {"offer", sdp::Type::OFFER},
        {"answer", sdp::Type::ANSWER},
        {"pranswer", sdp::Type::PRANSWER},
        {"rollback", sdp::Type::ROLLBACK}
    };
    auto it = type_map.find(type_string);
    return it != type_map.end() ? it->second : sdp::Type::UNSPEC;
}

std::string TypeToString(sdp::Type type) {
    switch (type) {
    case sdp::Type::UNSPEC:
        return "unspec";
    case sdp::Type::OFFER:
        return "offer";
    case sdp::Type::ANSWER:
        return "answer";
    case sdp::Type::PRANSWER:
        return "pranswer";
    case sdp::Type::ROLLBACK:
        return "rollback";
    default:
        return "unknown";
    }
}

}
}