#ifndef _RTC_BASE_UNITS_UNIT_BASE_H_
#define _RTC_BASE_UNITS_UNIT_BASE_H_

#include "base/defines.hpp"
#include "common/utils_numeric.hpp"

#include <stdint.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace naivertc {

// UnitBase is a base class for implementing custom value types with s specific unit.
// It provides type safety and commonly useful operations. The underlying storage is 
// always an int64_t, it's up to the unit implementation to choose what scale it represents.

template <class Unit_T>
class UnitBase {
public:
    static constexpr Unit_T Zero() { return Unit_T(0); }
    static constexpr Unit_T PlusInfinity() { return Unit_T(PlusInfinityVal()); }
    static constexpr Unit_T MinusInfinity() { return Unit_T(MinusInfinityVal()); }

public:
    UnitBase() = default;

    constexpr bool IsZero() const { return value_ == 0; }
    constexpr bool IsFinite() const { return !IsInfinite(); }
    constexpr bool IsInfinite() const { return IsPlusInfinity() || IsMinusInfinity(); }
    constexpr bool IsPlusInfinity() const { return value_ == PlusInfinityVal(); }
    constexpr bool IsMinusInfinity() const { return value_ == MinusInfinityVal(); }

    constexpr bool operator==(const Unit_T& other) const {
        return value_ == other.value_;
    }
    constexpr bool operator!=(const Unit_T& other) const {
        return value_ != other.value_;
    }
    constexpr bool operator<=(const Unit_T& other) const {
        return value_ <= other.value_;
    }
    constexpr bool operator>=(const Unit_T& other) const {
        return value_ >= other.value_;
    }
    constexpr bool operator>(const Unit_T& other) const {
        return value_ > other.value_;
    }
    constexpr bool operator<(const Unit_T& other) const {
        return value_ < other.value_;
    }
    constexpr Unit_T RoundTo(const Unit_T& resolution) const {
        assert(IsFinite());
        assert(resolution.IsFinite());
        assert(resolution.value_ > 0);
        return Unit_T((value_ + resolution.value_ / 2) / resolution.value_) *
            resolution.value_;
    }
    constexpr Unit_T RoundUpTo(const Unit_T& resolution) const {
        assert(IsFinite());
        assert(resolution.IsFinite());
        assert(resolution.value_ > 0);
        return Unit_T((value_ + resolution.value_ - 1) / resolution.value_) *
            resolution.value_;
    }
    constexpr Unit_T RoundDownTo(const Unit_T& resolution) const {
        assert(IsFinite());
        assert(resolution.IsFinite());
        assert(resolution.value_ > 0);
        return Unit_T(value_ / resolution.value_) * resolution.value_;
    }

protected:
    explicit constexpr UnitBase(int64_t value) : value_(value) {}

    template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
    static constexpr Unit_T FromValue(T value) {
        if (Unit_T::one_sided || std::is_unsigned<T>::value) {
            assert(value >= 0);
        } else {
            assert(value > MinusInfinityVal());
        }
        assert(value < PlusInfinityVal());
        return Unit_T(utils::numeric::checked_static_cast<int64_t>(value));
    }

    template <typename T,
              typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
    static constexpr Unit_T FromValue(T value) {
        if (value == std::numeric_limits<T>::infinity()) {
            return PlusInfinity();
        } else if (value == -std::numeric_limits<T>::infinity()) {
            return MinusInfinity();
        } else {
            assert(!std::isnan(value));
            return FromValue(utils::numeric::checked_static_cast<int64_t>(value));
        }
    }

    template <
        typename T,
        typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
    static constexpr Unit_T FromFraction(int64_t denominator, T value) {
        if (Unit_T::one_sided || std::is_unsigned<T>::value) {
            assert(value >= 0);
        } else {
            assert(value > MinusInfinityVal() / denominator);
        }
        assert(value < PlusInfinityVal() / denominator);
        return Unit_T(utils::numeric::checked_static_cast<int64_t>(value * denominator));
    }

    template <typename T,
              typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
    static constexpr Unit_T FromFraction(int64_t denominator, T value) {
        return FromValue(value * denominator);
    }

    template <typename T = int64_t>
    constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
    ToValue() const {
        assert(IsFinite());
        return utils::numeric::checked_static_cast<T>(value_);
    }
    template <typename T>
    constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
    ToValue() const {
        return IsPlusInfinity()
                ? std::numeric_limits<T>::infinity()
                : IsMinusInfinity() ? -std::numeric_limits<T>::infinity()
                                    : value_;
    }
    template <typename T>
    constexpr T ToValueOr(T fallback_value) const {
        return IsFinite() ? value_ : fallback_value;
    }

    template <int64_t Denominator, typename T = int64_t>
    constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
    ToFraction() const {
        assert(IsFinite());
        if (Unit_T::one_sided) {
            return utils::numeric::checked_static_cast<T>(DivRoundPositiveToNearest(value_, Denominator));
        } else {
            return utils::numeric::checked_static_cast<T>(DivRoundToNearest(value_, Denominator));
        }
    }
    template <int64_t Denominator, typename T>
    constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
    ToFraction() const {
        return ToValue<T>() * (1 / static_cast<T>(Denominator));
    }

    template <int64_t Denominator>
    constexpr int64_t ToFractionOr(int64_t fallback_value) const {
        return IsFinite() ? Unit_T::one_sided
                                ? DivRoundPositiveToNearest(value_, Denominator)
                                : DivRoundToNearest(value_, Denominator)
                        : fallback_value;
    }

    template <int64_t Factor, typename T = int64_t>
    constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
    ToMultiple() const {
        assert(ToValue() >= std::numeric_limits<T>::min() / Factor);
        assert(ToValue() <= std::numeric_limits<T>::max() / Factor);
        return utils::numeric::checked_static_cast<T>(ToValue() * Factor);
    }
    template <int64_t Factor, typename T>
    constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
    ToMultiple() const {
        return ToValue<T>() * Factor;
    }

private:
    static inline constexpr int64_t PlusInfinityVal() { return std::numeric_limits<int64_t>::max(); }
    static inline constexpr int64_t MinusInfinityVal() { return std::numeric_limits<int64_t>::min(); }

    constexpr Unit_T& AsSubClassRef() { return static_cast<Unit_T&>(*this); }
    constexpr const Unit_T& AsSubClassRef() const {
        return static_cast<const Unit_T&>(*this);
    }
    // Assumes that n >= 0 and d > 0.
    static constexpr int64_t DivRoundPositiveToNearest(int64_t n, int64_t d) {
        return (n + d / 2) / d;
    }
    // Assumes that d > 0.
    static constexpr int64_t DivRoundToNearest(int64_t n, int64_t d) {
        return (n + (n >= 0 ? d / 2 : -d / 2)) / d;
    }

private:
    template <class RelativeUnit_T>
    friend class RelativeUnit;

    int64_t value_;
};
    
} // namespace naivertc

#endif