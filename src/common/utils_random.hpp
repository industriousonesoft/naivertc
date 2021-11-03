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
    typename std::enable_if<std::is_floating_point<T>::value, T>::type* = nullptr>
RTC_CPP_EXPORT T generate_random() {
    std::mt19937 gen{std::random_device()()};
    std::uniform_real_distribution<T> dis;
    return dis(gen);
};

template <
    typename T, 
    typename std::enable_if<std::is_integral<T>::value, T>::type* = nullptr>
RTC_CPP_EXPORT T generate_random() {
    // auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    // std::default_random_engine generator(seed);
    std::mt19937 gen{std::random_device()()};
    std::uniform_int_distribution<T> dis;
    return dis(gen);
};

template <
    typename T, 
    typename std::enable_if<std::is_floating_point<T>::value, T>::type* = nullptr>
RTC_CPP_EXPORT T random(T lhs, T rhs) {
    assert(lhs <= rhs);
    std::mt19937 gen{std::random_device()()};
    std::uniform_real_distribution<T> dis(lhs, rhs);
    return dis(gen);
};

template <
    typename T, 
    typename std::enable_if<std::is_integral<T>::value, T>::type* = nullptr>
RTC_CPP_EXPORT T random(T lhs, T rhs) {
    assert(lhs <= rhs);
    std::mt19937 gen{std::random_device()()};
    std::uniform_int_distribution<T> dis(lhs, rhs);
    return dis(gen);
};

template<typename T> 
RTC_CPP_EXPORT void shuffle(std::vector<T> list){
    // auto seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    // std::default_random_engine gen(seed);
    std::mt19937 gen{std::random_device()()};
    std::shuffle(list.begin(), list.end(), gen);
};

// Random string
RTC_CPP_EXPORT std::string random_string(int max_length);

} // namespace random
} // namespace utils
} // naivertc

#endif