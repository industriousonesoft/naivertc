#ifndef _COMMON_UTILS_RANDOM_H_
#define _COMMON_UTILS_RANDOM_H_

#include "base/defines.hpp"

#include <random>
#include <chrono>
#include <vector>

namespace naivertc {
namespace utils {
namespace random {

template <
    typename T, 
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type
>
RTC_CPP_EXPORT T generate_random() {
    auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<T> uniform;
    return uniform(generator);
};

template<typename T> 
RTC_CPP_EXPORT void shuffle(std::vector<T> list){
    auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    std::shuffle(list.begin(), list.end(), std::default_random_engine(seed));
};

template <
    typename T, 
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type
>
RTC_CPP_EXPORT T random(T lhs, T rhs) {
    assert(lhs <= rhs);
    auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<T> uniform(lhs, rhs);
    return uniform(generator);
};

} // namespace random
} // namespace utils
} // naivertc

#endif