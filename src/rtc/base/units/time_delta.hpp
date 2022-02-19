#ifndef _RTC_BASE_UNITS_TIME_DELTA_H_
#define _RTC_BASE_UNITS_TIME_DELTA_H_

#include "base/defines.hpp"
#include "rtc/base/units/unit_relative.hpp"

#include <type_traits>

namespace naivertc {

// TimeDelta represents the difference between two timestamps. Commonly this can be 
// a duration. However since two Timestamps are not guaranteed to have the same epoch
// (they might come from different computers, make exact synchronisation infeasible.),
// the duration covered by a TimeDelta can be undefined. To simplify usage, it can be 
// constructed and converted to different units, specifically seconds(s), milliseconds(ms)
// microseconds (us).
class TimeDelta : public RelativeUnit<TimeDelta> {
public:
    template <typename T>
    static constexpr TimeDelta Seconds(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromFraction(1'000'000, value);
    }

    template <typename T>
    static constexpr TimeDelta Millis(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromFraction(1'000, value);
    }

    template <typename T>
    static constexpr TimeDelta Micros(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromValue(value);
    }

    TimeDelta() = delete;

    template <typename T = int64_t>
    constexpr T seconds() const {
        return ToFraction<1000000, T>();
    }
    template <typename T = int64_t>
    constexpr T ms() const {
        return ToFraction<1000, T>();
    }
    template <typename T = int64_t>
    constexpr T us() const {
        return ToValue<T>();
    }
    template <typename T = int64_t>
    constexpr T ns() const {
        return ToMultiple<1000, T>();
    }

    constexpr int64_t seconds_or(int64_t fallback_value) const {
        return ToFractionOr<1000000>(fallback_value);
    }
    constexpr int64_t ms_or(int64_t fallback_value) const {
        return ToFractionOr<1000>(fallback_value);
    }
    constexpr int64_t us_or(int64_t fallback_value) const {
        return ToValueOr(fallback_value);
    }

    constexpr TimeDelta Abs() const {
        return us() < 0 ? TimeDelta::Micros(-us()) : *this;
    }

private:
    friend class UnitBase<TimeDelta>;
    using RelativeUnit::RelativeUnit;
    static constexpr bool one_sided = false;

};
    
} // namespace naivertc


#endif