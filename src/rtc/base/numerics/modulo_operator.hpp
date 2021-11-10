#ifndef _RTC_BASE_NUMERICS_MOD_OPERATOR_H_
#define _RTC_BASE_NUMERICS_MOD_OPERATOR_H_

#include <algorithm>
#include <type_traits>

namespace naivertc {

template<unsigned long M>
inline unsigned long Add(unsigned long a, unsigned long b) {
    assert(a < M);
    unsigned long t = M - b % M;
    unsigned long res = a - t;
    if (t > a) {
        return res + M;
    }
    return res;
}

template <unsigned long M>
inline unsigned long Subtract(unsigned long a, unsigned long b) {
    assert(a < M);
    unsigned long sub = b % M;
    if (a < sub) {
        return M - (sub - a);
    }
    return a - sub;
}

// Calculates the forward difference between two wrapping numbers.
//
// Example:
// uint8_t x = 253;
// uint8_t y = 2;
//
// ForwardDiff(x, y) == 5
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
//          |----->----->----->----->----->
//
// ForwardDiff(y, x) == 251
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
// -->----->                              |----->---
//
// If M > 0 then wrapping occurs at M, if M == 0 then wrapping occurs at the
// largest value representable by T.
template <typename T, T M>
inline typename std::enable_if<(M > 0), T>::type ForwardDiff(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    assert(a < M);
    assert(b < M);
    return a <= b ? b - a : M - (a - b);
}

template <typename T, T M>
inline typename std::enable_if<(M == 0), T>::type ForwardDiff(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    return b - a;
}

template <typename T>
inline T ForwardDiff(T a, T b) {
    return ForwardDiff<T, 0>(a, b);
}

// Calculates the reverse difference between two wrapping numbers.
//
// Example:
// uint8_t x = 253;
// uint8_t y = 2;
//
// ReverseDiff(y, x) == 5
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
//          <-----<-----<-----<-----<-----|
//
// ReverseDiff(x, y) == 251
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
// ---<-----|                             |<-----<--
//
// If M > 0 then wrapping occurs at M, if M == 0 then wrapping occurs at the
// largest value representable by T.
template <typename T, T M>
inline typename std::enable_if<(M > 0), T>::type ReverseDiff(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    assert(a < M);
    assert(b < M);
    return b <= a ? a - b : M - (b - a);
}

template <typename T, T M>
inline typename std::enable_if<(M == 0), T>::type ReverseDiff(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    // Subtraction of two unsigned integer:
    // A - B = A + (B complement) = A +（~B+1) = A + (2^n-1-B+1) = A + (2^n-B)
    return a - b;
}

template <typename T>
inline T ReverseDiff(T a, T b) {
    return ReverseDiff<T, 0>(a, b);
}

// Calculates the minimum distance between to wrapping numbers.
//
// The minimum distance is defined as min(ForwardDiff(a, b), ReverseDiff(a, b))
template <typename T, T M = 0>
inline T MinDiff(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    return std::min(ForwardDiff<T, M>(a, b), ReverseDiff<T, M>(a, b));
}

} // namespace naivertc

#endif