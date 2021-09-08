#ifndef _RTC_BASE_UNITS_BIT_RATE_H_
#define _RTC_BASE_UNITS_BIT_RATE_H_

#include "base/defines.hpp"
#include "rtc/base/units/unit_relative.hpp"

namespace naivertc {

class RTC_CPP_EXPORT BitRate final : public RelativeUnit<BitRate> {
public:
    template <typename T>
    static constexpr BitRate BitsPerSec(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromValue(value);
    }
    template <typename T>
    static constexpr BitRate BytesPerSec(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromFraction(8, value);
    }
    template <typename T>
    static constexpr BitRate KilobitsPerSec(T value) {
        static_assert(std::is_arithmetic<T>::value, "");
        return FromFraction(1000, value);
    }
    static constexpr BitRate Infinity() { return MaxValue(); }

    BitRate() = delete;

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
    friend class UnitBase<BitRate>;
    using RelativeUnit::RelativeUnit;
    static constexpr bool one_sided = true;
};
    
} // namespace naivertc


#endif