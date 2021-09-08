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
class RTC_CPP_EXPORT Timestamp : public UnitBase<Timestamp> {
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
        if (IsMax() || delta.IsMax()) {
            assert(!IsMin());
            assert(!delta.IsMin());
            return MaxValue();
        } else if (IsMin() || delta.IsMin()) {
            assert(!IsMax());
            assert(!delta.IsMax());
            return MinValue();
        }
        return Timestamp::Micros(us() + delta.us());
    }
    constexpr Timestamp operator-(const TimeDelta delta) const {
        if (IsMax() || delta.IsMin()) {
            assert(!IsMin());
            assert(!delta.IsMax());
            return MaxValue();
        } else if (IsMin() || delta.IsMax()) {
            assert(!IsMax());
            assert(!delta.IsMin());
            return MinValue();
        }
        return Timestamp::Micros(us() - delta.us());
    }

    constexpr TimeDelta operator-(const Timestamp other) const {
        if (IsMax() || other.IsMin()) {
            assert(!IsMin());
            assert(!other.IsMax());
            return TimeDelta::MaxValue();
        } else if (IsMin() || other.IsMax()) {
            assert(!IsMax());
            assert(!other.IsMin());
            return TimeDelta::MinValue();
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
    static constexpr bool one_sided = true;
};
    
} // namespace naivertc


#endif