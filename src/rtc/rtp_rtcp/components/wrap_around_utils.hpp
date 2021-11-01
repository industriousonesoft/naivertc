#ifndef _RTC_RTP_RTCP_COMPONENTS_SEQ_NUM_UTILS_H_
#define _RTC_RTP_RTCP_COMPONENTS_SEQ_NUM_UTILS_H_

#include "rtc/base/numerics/modulo_operator.hpp"

#include <limits>
#include <type_traits>

namespace naivertc {
namespace wrap_around_utils {

// Check if the sequence number `a` is ahead or at sequence number `b`.
// NOTE: Same as `IsNewer` in wrap_around_checker.hpp
template <typename T, T M>
inline typename std::enable_if<(M > 0), bool>::type AheadOrAt(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    // If `M` is an even number and the two sequence numbers are at max distance
    // from each other, then the sequence number with the highest value is
    // considered to be ahead.
    const T max_dist = M / 2;
    if (!(M & 1) && MinDiff<T, M>(a, b) == max_dist) {
        return b < a;
    }
    return ForwardDiff<T, M>(b, a) <= max_dist;
}

template <typename T, T M>
inline typename std::enable_if<(M == 0), bool>::type AheadOrAt(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    const T maxDist = std::numeric_limits<T>::max() / 2 + T(1);
    if (a - b == maxDist)
        return b < a;
    return ForwardDiff(b, a) < maxDist;
}

template <typename T>
inline bool AheadOrAt(T a, T b) {
    return AheadOrAt<T, 0>(a, b);
}

// Check if the sequence number `a` is ahead of sequence number `b`.
template <typename T, T M = 0>
inline bool AheadOf(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "Type must be an unsigned integer.");
    return a != b && AheadOrAt<T, M>(a, b);
}

// Functor which returns true if the `a` is newer than `b`.
//
// WARNING! If used to sort sequence numbers of length M then the interval
//          covered by the sequence numbers may not be larger than floor(M/2).
template <typename T, T M = 0>
struct NewerThan {
    bool operator()(T a, T b) const { return AheadOf<T, M>(a, b); }
};

// Functor which returns true if the `a` is older than `b`.
//
// WARNING! If used to sort sequence numbers of length M then the interval
//          covered by the sequence numbers may not be larger than floor(M/2).
template <typename T, T M = 0>
struct OlderThan {
    bool operator()(T a, T b) const { return AheadOf<T, M>(b, a); }
};

} // namespace wrap_around_utils
} // namespace naivertc


#endif