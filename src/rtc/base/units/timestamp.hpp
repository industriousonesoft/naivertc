#ifndef _RTC_BASE_UNITS_TIMESTAMP_H_
#define _RTC_BASE_UNITS_TIMESTAMP_H_

#include "base/defines.hpp"
#include "rtc/base/units/unit_base.hpp"
#include "rtc/base/units/time_delta.hpp"

#include <type_traits>

namespace naivertc {

// Timestamp represents the time that has passed since some unspecifiec epoch.
// The epoch is assumed to be before any represented timestamps, this means that
// negative value are not valid. The most notable feature is that the difference of 
// two Timestamps results in a TimeDelta.
class Timestamp : public UnitBase<Timestamp> {
public:
    template <typename T>
    static constexpr Timestamp Seconds(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromFraction(1'000'000, value);
    }

    template <typename T>
    static constexpr Timestamp Millis(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromFraction(1'000, value);
    }

    template <typename T>
    static constexpr Timestamp Micros(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromValue(value);
    }

    Timestamp() = delete;

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

    constexpr int64_t seconds_or(int64_t fallback_value) const {
        return ToFractionOr<1000000>(fallback_value);
    }
    constexpr int64_t ms_or(int64_t fallback_value) const {
        return ToFractionOr<1000>(fallback_value);
    }
    constexpr int64_t us_or(int64_t fallback_value) const {
        return ToValueOr(fallback_value);
    }

    constexpr Timestamp operator+(const TimeDelta delta) const {
        if (IsPlusInfinity() || delta.IsPlusInfinity()) {
            assert(!IsMinusInfinity());
            assert(!delta.IsMinusInfinity());
            return PlusInfinity();
        } else if (IsMinusInfinity() || delta.IsMinusInfinity()) {
            assert(!IsPlusInfinity());
            assert(!delta.IsPlusInfinity());
            return MinusInfinity();
        }
        return Timestamp::Micros(us() + delta.us());
    }
    constexpr Timestamp operator-(const TimeDelta delta) const {
        if (IsPlusInfinity() || delta.IsMinusInfinity()) {
            assert(!IsMinusInfinity());
            assert(!delta.IsPlusInfinity());
            return PlusInfinity();
        } else if (IsMinusInfinity() || delta.IsPlusInfinity()) {
            assert(!IsPlusInfinity());
            assert(!delta.IsMinusInfinity());
            return MinusInfinity();
        }
        return Timestamp::Micros(us() - delta.us());
    }

    constexpr TimeDelta operator-(const Timestamp other) const {
        if (IsPlusInfinity() || other.IsMinusInfinity()) {
            assert(!IsMinusInfinity());
            assert(!other.IsPlusInfinity());
            return TimeDelta::PlusInfinity();
        } else if (IsMinusInfinity() || other.IsPlusInfinity()) {
            assert(!IsPlusInfinity());
            assert(!other.IsMinusInfinity());
            return TimeDelta::MinusInfinity();
        }
        return TimeDelta::Micros(us() - other.us());
    }
    
    constexpr Timestamp& operator-=(const TimeDelta delta) {
        *this = *this - delta;
        return *this;
    }
    constexpr Timestamp& operator+=(const TimeDelta delta) {
        *this = *this + delta;
        return *this;
    }

private:
    friend class UnitBase<Timestamp>;
    using UnitBase::UnitBase;
    // TODO: Using uint64_t instread of int64_t to represent the unit is one-sided property
    static constexpr bool one_sided = true;
};
    
} // namespace naivertc


#endif