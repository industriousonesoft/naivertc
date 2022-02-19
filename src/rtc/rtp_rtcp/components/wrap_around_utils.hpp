#ifndef _RTC_RTP_RTCP_COMPONENTS_SEQ_NUM_UTILS_H_
#define _RTC_RTP_RTCP_COMPONENTS_SEQ_NUM_UTILS_H_

#include "base/defines.hpp"
#include "rtc/base/numerics/modulo_operator.hpp"

#include <limits>
#include <type_traits>

namespace naivertc {
namespace wrap_around_utils {

// Check if th number `a` is ahead or at number `b`.
template <typename T, T M>
inline typename std::enable_if<(M > 0), bool>::type AheadOrAt(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    // If `M` is an even number and the two numbers are at max distance
    // from each other, then the number with the highest value is
    // considered to be ahead.
    const T max_dist = M / 2;
    const bool is_even = (M & 1) == 0;
    if (is_even && MinDiff<T, M>(a, b) == max_dist) {
        return b < a;
    }
    return ForwardDiff<T, M>(b, a) <= max_dist;
}

template <typename T, T M>
inline typename std::enable_if<(M == 0), bool>::type AheadOrAt(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    // `max_dist` is the half-way mark (the half count of all count that the tyep U can represent) for the type U. 
    // For instance, for a uint16_t it will be 0x8000 (2^15), and for a uint32_t, it will be 0x80000000 (2^31).
    const T max_dist = (std::numeric_limits<T>::max() / 2) + T(1);
    if (a - b == max_dist)
        return b < a;
    return ForwardDiff(b, a) < max_dist;
}

template <typename T>
inline bool AheadOrAt(T a, T b) {
    return AheadOrAt<T, 0>(a, b);
}

// Check if the number `a` is ahead of number `b`.
template <typename T, T M = 0>
inline bool AheadOf(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    return a != b && AheadOrAt<T, M>(a, b);
}

// Return the latest one.
template <typename T, T M = 0>
inline T Latest(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    return AheadOf<T, M>(a, b) ? a : b;
}

template <typename T, T M = 0>
// Return 0 on no wrap around, 1 on forward wrap around, -1 on backward wrap around.
inline int DetectWrapAround(T prev, T curr) {
    // NOTE: A new way to detect wrap around: curr < 0x0000ffff && prev > 0xffff0000
    if (curr < prev) {
        // When the incoming timestamp is less than the previous one, there are probably two situation:
        // it's a froward wrap around if the forward difference is less than 2^31 (casting to a int32_t, it should be positive),
        // otherwise, it's a backwrad. (e.g. timestamp = 1, _prevTimestamp = 2^32 - 1).
        if (static_cast<int32_t>(ForwardDiff(prev, curr)) > 0) {
            // Forward wrap around
            return 1;
        }
    } else {
        // When the incoming timestamp is greater than the previous one, there are probably two situation:
        // it's a backward wrap around if the reverse difference is less than 2^31 (casting to a int32_t, it should be positive),
        // otherwise, it's a forward. (e.g. timestamp = 2^32 - 1, _prevTimestamp = 1).
        if (static_cast<int32_t>(ReverseDiff(prev, curr)) > 0) {
            // Backward wrap around
            return -1;
        }
    }
    return 0;
}

// Functor which returns true if the `a` is newer than `b`.
//
// WARNING! If used to sort numbers of length M then the interval
//          covered by the numbers may not be larger than floor(M/2).
template <typename T, T M = 0>
struct NewerThan {
    bool operator()(T a, T b) const { return AheadOf<T, M>(a, b); }
};

// Functor which returns true if the `a` is older than `b`.
//
// WARNING! If used to sort numbers of length M then the interval
//          covered by the numbers may not be larger than floor(M/2).
template <typename T, T M = 0>
struct OlderThan {
    bool operator()(T a, T b) const { return AheadOf<T, M>(b, a); }
};

} // namespace wrap_around_utils
} // namespace naivertc


#endif