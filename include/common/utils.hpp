#ifndef _COMMON_STR_UTILS_H_
#define _COMMON_STR_UTILS_H_

#include <string>

namespace utils {

// string
namespace string {

bool match_prefix(const std::string_view str, const std::string_view prefix);
void trim_begin(std::string &str);
void trim_end(std::string &str);
std::pair<std::string_view, std::string_view> parse_pair(std::string_view attr);
template<typename T> T to_integer(std::string_view s);
bool is_sha256_fingerprint(std::string_view fingerprint);

}

// random
namespace random {

// TODO: how to define a template with numeric constraint
template<typename T> T generate_random();

}

}

#endif