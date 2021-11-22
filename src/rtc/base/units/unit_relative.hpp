#ifndef _RTC_BASE_UNITS_UNIT_RELATIVE_H_
#define _RTC_BASE_UNITS_UNIT_RELATIVE_H_

#include "base/defines.hpp"
#include "rtc/base/units/unit_base.hpp"

namespace naivertc {

// Extends UnitBase to provide operations for relative units, that is, units
// that have meningful relation between values such that a += b is a sensible
// thing to do.

template <class Unit_T> 
class RelativeUnit : public UnitBase<Unit_T> {
public:
    constexpr Unit_T Clamped(Unit_T min_value, Unit_T max_value) const {
        return std::max(min_value,
                        std::min(UnitBase<Unit_T>::AsSubClassRef(), 
                        max_value));
    }
    
    constexpr void Clamp(Unit_T min_value, Unit_T max_value) {
        *this = Clamped(min_value, max_value);
    }

    constexpr Unit_T operator+(const Unit_T other) const {
        if (this->IsMax() || other.IsMax()) {
            assert(!this->IsMin());
            assert(!other.IsMin());
            return this->PlusInfinity();
        } else if (this->IsMin() || other.IsMin()) {
            assert(!this->IsMax());
            assert(!other.IsMax());
            return this->MinusInfinity();
        }
        return UnitBase<Unit_T>::FromValue(this->ToValue() + other.ToValue());
    }

    constexpr Unit_T operator-(const Unit_T other) const {
        if (this->IsMax() || other.IsMin()) {
            assert(!this->IsMin());
            assert(!other.IsMax());
            return this->PlusInfinity();
        } else if (this->IsMin() || other.IsMax()) {
            assert(!this->IsMax());
            assert(!other.IsMin());
            return this->MinusInfinity();
        }
        return UnitBase<Unit_T>::FromValue(this->ToValue() - other.ToValue());
    }
    constexpr Unit_T& operator+=(const Unit_T other) {
        *this = *this + other;
        return this->AsSubClassRef();
    }
    constexpr Unit_T& operator-=(const Unit_T other) {
        *this = *this - other;
        return this->AsSubClassRef();
    }
    constexpr double operator/(const Unit_T other) const {
        return UnitBase<Unit_T>::template ToValue<double>() /
            other.template ToValue<double>();
    }
    template <typename T>
    constexpr typename std::enable_if<std::is_arithmetic<T>::value, Unit_T>::type
    operator/(const T& scalar) const {
        return UnitBase<Unit_T>::FromValue(
            std::round(UnitBase<Unit_T>::template ToValue<int64_t>() / scalar));
    }
    constexpr Unit_T operator*(double scalar) const {
        return UnitBase<Unit_T>::FromValue(std::round(this->ToValue() * scalar));
    }
    constexpr Unit_T operator*(int64_t scalar) const {
        return UnitBase<Unit_T>::FromValue(this->ToValue() * scalar);
    }
    constexpr Unit_T operator*(int32_t scalar) const {
        return UnitBase<Unit_T>::FromValue(this->ToValue() * scalar);
    }
protected:
    using UnitBase<Unit_T>::UnitBase;
};
    
} // namespace naivertc


#endif