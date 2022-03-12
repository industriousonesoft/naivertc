#ifndef _RTC_BASE_UNITS_BIT_RATE_H_
#define _RTC_BASE_UNITS_BIT_RATE_H_

#include "base/defines.hpp"
#include "rtc/base/units/unit_relative.hpp"
#include "rtc/base/units/time_delta.hpp"

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

// Helper methods
inline constexpr DataRate operator/(const size_t size_in_bytes, const TimeDelta& duration) {
    return DataRate::BitsPerSec(size_in_bytes * 8000'000 / duration.us());
}

inline constexpr TimeDelta operator/(const size_t size_in_bytes, const DataRate& rate) {
    return TimeDelta::Micros(size_in_bytes * 8000'000 / rate.bps());
}

inline constexpr size_t operator*(const TimeDelta& duration, const DataRate& rate) {
    return static_cast<size_t>(((rate.bps() * duration.us()) + 4000'000) / 8000'000);
}

inline constexpr size_t operator*(const DataRate& rate, const TimeDelta& duration) {
    return duration * rate;
}
    
} // namespace naivertc


#endif