#include "common/utils.hpp"

namespace naivertc {
namespace utils {

// numberic
namespace numeric {
}

// string
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

} // enamespace string

// Random
namespace random {} // namespace random

}
}