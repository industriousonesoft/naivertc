#ifndef _COMMON_UTILS_NUMERIC_H_
#define _COMMON_UTILS_NUMERIC_H_

#include "base/defines.hpp"
#include "common/numeric_range_checker.hpp"

#include <type_traits>

namespace naivertc {
namespace utils {

// numeric
namespace numeric {

template <typename Dst, typename Src>
RTC_CPP_EXPORT inline constexpr bool is_value_in_range(Src value) {
    return RangeCheck<Dst>(value) == RangeCheckResult::TYPE_VALID;
};

template <
    typename T,
    // 只有当第一个模板参数: std::is_integral<T>::value = true 时，则将类型T启用为成员类型enable_if :: type，未定义enable_if :: type，产生编译错误
    typename = typename std::enable_if<std::is_integral<T>::value, T>::type>
RTC_CPP_EXPORT uint16_t to_uint16(T i) {
    if (is_value_in_range<uint16_t>(i))
		return static_cast<uint16_t>(i);
	else
		throw std::invalid_argument("Integer out of range");
};

template <
    typename T,
    typename = typename std::enable_if<std::is_integral<T>::value, T>::type>
RTC_CPP_EXPORT uint32_t to_uint32(T i) {
    if (is_value_in_range<uint32_t>(i)) {
        return static_cast<uint32_t>(i);
    }else {
        throw std::invalid_argument("Integer out of range.");
    }
};

template <typename Dst, typename Src>
RTC_CPP_EXPORT inline constexpr Dst checked_static_cast(Src value) {
    assert(is_value_in_range<Dst>(value));
    return static_cast<Dst>(value);
}

}

} // namespace utils
} // namespace naivertc

#endif