#include "rtc/sdp/sdp_utils.hpp"
#include "common/utils.hpp"

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

std::string RoleToString(sdp::Role role) {
    switch (role) {
    case sdp::Role::ACT_PASS:
        return "actpass";
    case sdp::Role::PASSIVE:
        return "passive";
    case sdp::Role::ACTIVE:
        return "active";
    default:
        return "unknown";
    }
}

std::optional<std::string> ParseFingerprintAttribute(std::string_view value) {
    if (utils::string::match_prefix(value, "sha-256")) {
        std::string fingerprint{value.substr(7)};
        utils::string::trim_begin(fingerprint);
        return std::make_optional<std::string>(fingerprint);
    }else {
        return std::nullopt;
    }
}

// a=fingerprint:sha-256 A9:CA:95:47:CB:8D:81:DE:E4:78:38:1E:70:6B:AA:14:66:6C:AF:7F:89:D7:B7:C7:1A:A9:45:09:83:CC:0D:03
// 常规的SHA256哈希值是一个长度为32个字节的数组，通常用一个长度为64的十六进制字符串来表示
// SDP中的fingerprint在每两个个字节之间加入了一个间隔符”:“，因此长度=32 * 2 +（32 - 1）
constexpr int kSHA256FixedLength = 32 * 3 - 1;
bool IsSHA256Fingerprint(std::string_view fingerprint) {
    if (fingerprint.size() != kSHA256FixedLength) {
        return false;
    }

    for (size_t i = 0; i < fingerprint.size(); ++i) {
        if (i % 3 == 2) {
            if (fingerprint[i] != ':') {
                return false;
            }
        }else {
            if (!std::isxdigit(fingerprint[i])) {
                return false;
            }
        }
    }
    return true;
}

} // namespace sdp
} // namespace naivertc