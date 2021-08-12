#ifndef _RTC_BASE_DIVIDE_ROUND_H_
#define _RTC_BASE_DIVIDE_ROUND_H_

#include "base/defines.hpp"

namespace naivertc {

template <typename Dividend, typename Divisor>
inline auto constexpr DivideRoundUp(Dividend dividend, Divisor divisor) {
    static_assert(std::is_integral<Dividend>(), "");
    static_assert(std::is_integral<Divisor>(), "");
    assert(dividend >= 0);
    assert(divisor > 0);

    auto quotient = dividend / divisor;
    auto remainder = dividend % divisor;
    return quotient + (remainder > 0 ? 1 : 0);
}

template <typename Dividend, typename Divisor>
inline auto constexpr DivideRoundToNearest(Dividend dividend, Divisor divisor) {
    static_assert(std::is_integral<Dividend>(), "");
    static_assert(std::is_integral<Divisor>(), "");
    assert(dividend >= 0);
    assert(divisor > 0);

    auto half_of_divisor = (divisor - 1) / 2;
    auto quotient = dividend / divisor;
    auto remainder = dividend % divisor;

    return quotient + (remainder > half_of_divisor ? 1 : 0);
}

    
} // namespace naivertc


#endif