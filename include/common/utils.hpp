#ifndef _COMMON_STR_UTILS_H_
#define _COMMON_STR_UTILS_H_

#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <random>
#include <chrono>
#include <type_traits>
#include <limits>


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

// weak_bind
template <typename F, typename T, typename... Args> auto weak_bind(F&& f, T* t, Args&& ..._args) {
    return [bound = std::bind(f, t, _args...), weak_this = t->weak_from_this()](auto &&...args) {
        if (auto shared_this = weak_this.lock()) {
            return bound(args...);
        }else {
            return static_cast<decltype(bound(args...))>(false);
        }
    };
}

// numeric
namespace numeric {

template<
    typename T,
    // 只有当第一个模板参数: std::is_integral<T>::value = true 时，则将类型T启用为成员类型enable_if :: type，未定义enable_if :: type，产生编译错误
    typename = typename std::enable_if<std::is_integral<T>::value, T>::type
>
uint16_t to_uint16(T i) {
    if (i >= 0 && static_cast<typename std::make_unsigned<T>::type>(i) <= std::numeric_limits<uint16_t>::max())
		return static_cast<uint16_t>(i);
	else
		throw std::invalid_argument("Integer out of range");
};

template<
    typename T,
    typename = typename std::enable_if<std::is_integral<T>::value, T>::type
>
uint32_t to_uint32(T i) {
    if (i >=0 && static_cast<typename std::make_unsigned<T>::type>(i) <= std::numeric_limits<uint32_t>::max()) {
        return static_cast<uint32_t>(i);
    }else {
        throw std::invalid_argument("Integer out of range.");
    }
};

}

// string
namespace string {

bool match_prefix(const std::string_view str, const std::string_view prefix);
void trim_begin(std::string &str);
void trim_end(std::string &str);
std::pair<std::string_view, std::string_view> parse_pair(std::string_view attr);

template<typename T> T to_integer(std::string_view s) {
      const std::string str(s);
    try {
        return std::is_signed<T>::value ? T(std::stol(str)) : T(std::stoul(str));
    } catch(...) {
        throw std::invalid_argument("Invalid integer \"" + str + "\" in description");
    }
};

}

// random
namespace random {

template <
    typename T, 
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type
>
T generate_random() {
    auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<T> uniform;
    return uniform(generator);
};

template<typename T> 
void shuffle(std::vector<T> list){
    auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    std::shuffle(list.begin(), list.end(), std::default_random_engine(seed));
};

} 

} // namespace utils
} // namespace naivertc

#endif