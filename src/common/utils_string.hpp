#ifndef _COMMON_UTILS_STRING_H_
#define _COMMON_UTILS_STRING_H_

#include "base/defines.hpp"

#include <string>

namespace naivertc {
namespace utils {
namespace string {

bool match_prefix(std::string_view str, std::string_view prefix);
void trim_begin(std::string &str);
void trim_end(std::string &str);
std::pair<std::string_view, std::string_view> parse_pair(std::string_view attr);

template<typename T> 
T to_integer(std::string_view s) {
      const std::string str(s);
    try {
        return std::is_signed<T>::value ? T(std::stol(str)) : T(std::stoul(str));
    } catch(...) {
        throw std::invalid_argument("Invalid integer \"" + str + "\" in description");
    }
};

} // namespace string
} // namespace utils
} // namespace naivertc

#endif