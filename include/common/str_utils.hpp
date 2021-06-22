#ifndef _COMMON_STR_UTILS_H_
#define _COMMON_STR_UTILS_H_

#include <string>

namespace utils {

bool MatchPrefix(const std::string_view str, const std::string_view prefix);
void TrimBegin(std::string &str);
void TrimEnd(std::string &str);
std::pair<std::string_view, std::string_view> ParsePair(std::string_view attr);
template<typename T> T ToInteger(std::string_view s);
bool isSHA256FingerPrint(std::string_view s);

}

#endif