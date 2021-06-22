#include "common/str_utils.hpp"

#include <sstream>

namespace utils {

bool MatchPrefix(const std::string &str, const std::string &prefix) {
    return str.size() >= prefix.size() && std::mismatch(prefix.begin(), prefix.end(), str.begin()).first == prefix.end();
}

void TrimBegin(std::string &str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](char c){
        return !std::isspace(c);
    }));
}

void TrimEnd(std::string &str) {
    // reverse_iterator.base() -> iterator
    str.erase(std::find_if(str.rbegin(), str.rend(), [](char c){
        return !std::isspace(c);
    }).base(), str.end());
}

std::pair<std::string_view, std::string_view> ParsePair(std::string_view attr) {
    std::string_view key, value;
    if (size_t separator = attr.find('.'); separator != std::string::npos) {
        key = attr.substr(0, separator);
        value = attr.substr(separator+1);
    }else {
        key = attr;
    }
    return std::make_pair(std::move(key), std::move(value));
}

template<typename T> T ToInteger(std::string_view s) {
    const std::string str(s);
    try {
        return std::is_signed<T>::value ? T(std::stol(str)) : T(std::stoul(str));
    } catch(...) {
        throw std::invalid_argument("Invalid integer \"" + str + "\" in description");
    }
}

// a=fingerprint:sha-256 A9:CA:95:47:CB:8D:81:DE:E4:78:38:1E:70:6B:AA:14:66:6C:AF:7F:89:D7:B7:C7:1A:A9:45:09:83:CC:0D:03
// 常规的SHA256哈希值是一个长度为32个字节的数组，通常用一个长度为64的十六进制字符串来表示
// SDP中的fingerprint在每两个个字节之间加入了一个间隔符”:“，因此长度=32 * 2 +（32 - 1）
constexpr int kSHA256FixedLength = 32 * 3 - 1;
bool isSHA256FingerPrint(std::string_view finger_print) {
    if (finger_print.size() != kSHA256FixedLength) {
        return false;
    }

    for (size_t i = 0; i < finger_print.size(); ++i) {
        if (i % 3 == 2) {
            if (finger_print[i] != ':') {
                return false;
            }
        }else {
            if (!std::isxdigit(finger_print[i])) {
                return false;
            }
        }
    }
    return true;
}

}