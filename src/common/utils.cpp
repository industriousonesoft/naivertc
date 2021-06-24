#include "common/utils.hpp"

#include <sstream>
#include <random>
#include <chrono>
#include <type_traits>

namespace utils {

namespace string {
bool match_prefix(const std::string_view str, const std::string_view prefix) {
    return str.size() >= prefix.size() && std::mismatch(prefix.begin(), prefix.end(), str.begin()).first == prefix.end();
}

void trim_begin(std::string &str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](char c){
        return !std::isspace(c);
    }));
}

void trim_end(std::string &str) {
    // reverse_iterator.base() -> iterator
    str.erase(std::find_if(str.rbegin(), str.rend(), [](char c){
        return !std::isspace(c);
    }).base(), str.end());
}

std::pair<std::string_view, std::string_view> parse_pair(std::string_view attr) {
    std::string_view key, value;
    if (size_t separator = attr.find(':'); separator != std::string::npos) {
        key = attr.substr(0, separator);
        value = attr.substr(separator+1);
    }else {
        key = attr;
    }
    return std::make_pair(std::move(key), std::move(value));
}

template<typename T> T to_integer(std::string_view s) {
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
bool is_sha256_fingerprint(std::string_view fingerprint) {
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

} // end of string namespace

// Random
namespace random {

template<typename T> T generate_random() {
    auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<T> uniform;
    return uniform(generator);
}

template<typename T> void shuffle(std::vector<T> list) {
    auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    std::shuffle(list.begin(), list.end(), std::default_random_engine(seed));
}

} // end of random namespace

}