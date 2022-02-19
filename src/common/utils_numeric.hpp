#ifndef _COMMON_UTILS_NUMERIC_H_
#define _COMMON_UTILS_NUMERIC_H_

#include "base/defines.hpp"
#include "common/numeric_range_checker.hpp"

#include <type_traits>

namespace naivertc {
namespace utils {
namespace numeric {

// is_value_in_range
template <typename Dst, typename Src>
inline constexpr bool is_value_in_range(Src value) {
    return RangeCheck<Dst>(value) == RangeCheckResult::TYPE_VALID;
};

template <
    typename T,
    // 只有当第一个模板参数: std::is_integral<T>::value = true 时，则将类型T启用为成员类型enable_if :: type，未定义enable_if :: type，产生编译错误
    typename = typename std::enable_if<std::is_integral<T>::value, T>::type>
uint16_t to_uint16(T i) {
    if (is_value_in_range<uint16_t>(i))
		return static_cast<uint16_t>(i);
	else
		throw std::invalid_argument("Integer out of range");
};

template <
    typename T,
    typename = typename std::enable_if<std::is_integral<T>::value, T>::type>
uint32_t to_uint32(T i) {
    if (is_value_in_range<uint32_t>(i)) {
        return static_cast<uint32_t>(i);
    } else {
        throw std::invalid_argument("Integer out of range.");
    }
};

// checked_static_cast
template <typename Dst, typename Src>
inline constexpr Dst checked_static_cast(Src value) {
    assert(is_value_in_range<Dst>(value));
    return static_cast<Dst>(value);
}

// division_with_roundup
template <typename T,
          typename std::enable_if<std::is_floating_point<T>::value, T>::type* = nullptr>
inline T division_with_roundup(T numerator, T denominator) {
    assert(denominator > 0);
    return numerator / denominator + 0.5;
}

template <typename T,
          typename std::enable_if<std::is_integral<T>::value, T>::type* = nullptr>
inline T division_with_roundup(T numerator, T denominator) {
    assert(denominator > 0);
    return (numerator + denominator / 2) / denominator;
}

}

} // namespace utils
} // namespace naivertc

#endif