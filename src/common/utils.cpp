#include "common/utils.hpp"

#include <sstream>
#include <random>
#include <chrono>
#include <type_traits>
#include <limits>

namespace naivertc {
namespace utils {

// numberic
namespace numeric {

template<typename T>
uint16_t to_uint16(T i) {
    if (i >= 0 && static_cast<typename std::make_unsigned<T>::type>(i) <= std::numeric_limits<uint16_t>::max())
		return static_cast<uint16_t>(i);
	else
		throw std::invalid_argument("Integer out of range");
}


template<typename T>
uint16_t to_uint32(T i) {
    if (i >=0 && static_cast<typename std::make_unsigned<T>::type>(i) <= std::numeric_limits<uint32_t>::max()) {
        return static_cast<uint32_t>(i);
    }else {
        throw std::invalid_argument("Integer out of range.");
    }
}

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

template<typename T> T to_integer(std::string_view s) {
    const std::string str(s);
    try {
        return std::is_signed<T>::value ? T(std::stol(str)) : T(std::stoul(str));
    } catch(...) {
        throw std::invalid_argument("Invalid integer \"" + str + "\" in description");
    }
}

} // enamespace string

// Random
namespace random {

template <typename T>
T generate_random() {
    auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<T> uniform;
    return uniform(generator);
}

template<typename T> 
void shuffle(std::vector<T> list) {
    auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    std::shuffle(list.begin(), list.end(), std::default_random_engine(seed));
}

} // namespace random

}
}