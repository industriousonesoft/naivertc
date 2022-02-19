#ifndef _RTC_BASE_UNITS_BIT_RATE_H_
#define _RTC_BASE_UNITS_BIT_RATE_H_

#include "base/defines.hpp"
#include "rtc/base/units/unit_relative.hpp"

namespace naivertc {

class DataRate final : public RelativeUnit<DataRate> {
public:
    template <typename T>
    static constexpr DataRate BitsPerSec(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromValue(value);
    }
    template <typename T>
    static constexpr DataRate BytesPerSec(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromFraction(8, value);
    }
    template <typename T>
    static constexpr DataRate KilobitsPerSec(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromFraction(1000, value);
    }
    static constexpr DataRate Infinity() { return PlusInfinity(); }

    DataRate() = delete;

    template <typename T = int64_t>
    constexpr T bps() const {
        return ToValue<T>();
    }
    template <typename T = int64_t>
    constexpr T bytes_per_sec() const {
        return ToFraction<8, T>();
    }
    template <typename T = int64_t>
    constexpr T kbps() const {
        return ToFraction<1000, T>();
    }
    constexpr int64_t bps_or(int64_t fallback_value) const {
        return ToValueOr(fallback_value);
    }
    constexpr int64_t kbps_or(int64_t fallback_value) const {
        return ToFractionOr<1000>(fallback_value);
    }
private:
    friend class UnitBase<DataRate>;
    using RelativeUnit::RelativeUnit;
    static constexpr bool one_sided = true;
};
    
} // namespace naivertc


#endif