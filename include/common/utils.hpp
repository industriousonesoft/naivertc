#ifndef _COMMON_STR_UTILS_H_
#define _COMMON_STR_UTILS_H_

#include <string>
#include <vector>
#include <functional>

namespace naivertc {
namespace utils {

// instanceof
template<typename Base, typename T>
inline bool instanceof(const T*) {
   return std::is_base_of<Base, T>::value;
}

// TODO: Overload Pattern in C++17，overload原理就是模板推导和转发，变参模板怎么理解？
/** eg:
struct overloadInt{ 
    void operator(int arg){
        std::cout<<arg<<' ';
    } 
};
struct overload : overloadInt{
    using overloadInt::operator();
};
*/
// overloaded helper
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// numeric
namespace numeric {

template<
    typename T,
    typename = typename std::enable_if<std::is_integral<T>::value, T>::type
>
uint16_t to_uint16(T i);

template<
    typename T,
    typename = typename std::enable_if<std::is_integral<T>::value, T>::type
>
uint32_t to_uint32(T i);

}

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

template <
    typename T, 
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type
>
T generate_random();

template<typename T> 
void shuffle(std::vector<T> list);

}


}
}

#endif