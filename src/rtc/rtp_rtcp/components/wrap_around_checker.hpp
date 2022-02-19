#ifndef _RTC_RTP_RTCP_COMPONENTS_NUMBER_WRAP_CHECKER_H_
#define _RTC_RTP_RTCP_COMPONENTS_NUMBER_WRAP_CHECKER_H_

#include "base/defines.hpp"

#include <limits>

namespace naivertc {

// NOTE: This class was deprecated, please use `wrap_around_utils` instead.
template <typename U>
RTC_DEPRECATED("Use wrap_around_utils instead")
inline bool IsNewer(U value, U prev_value) {
    static_assert(!std::numeric_limits<U>::is_signed, "U must be unsigned");
    // kBreakpoint is the half-way mark for the type U. For instance, for a
    // uint16_t it will be 0x8000 (2^15), and for a uint32_t, it will be 0x80000000 (2^31).
    constexpr U kBreakpoint = (std::numeric_limits<U>::max() >> 1) + 1;
    // Distinguish between elements that are exactly kBreakpoint apart.
    // If t1>t2 and |t1-t2| = kBreakpoint: IsNewer(t1,t2)=true,
    // IsNewer(t2,t1)=false
    // rather than having IsNewer(t1,t2) = IsNewer(t2,t1) = false.
    if (value - prev_value == kBreakpoint) {
        return value > prev_value;
    }
    return value != prev_value &&
            static_cast<U>(value - prev_value) < kBreakpoint;
}

RTC_DEPRECATED("Use wrap_around_utils instead")
inline bool IsNewerSequenceNumber(uint16_t sequence_number,
                                  uint16_t prev_sequence_number) {
    return IsNewer(sequence_number, prev_sequence_number);
}

RTC_DEPRECATED("Use wrap_around_utils instead")
inline bool IsNewerTimestamp(uint32_t timestamp, uint32_t prev_timestamp) {
    return IsNewer(timestamp, prev_timestamp);
}

RTC_DEPRECATED("Use wrap_around_utils instead")
inline uint16_t LatestSequenceNumber(uint16_t sequence_number1,
                                     uint16_t sequence_number2) {
    return IsNewerSequenceNumber(sequence_number1, sequence_number2) ? sequence_number1 : sequence_number2;
}

RTC_DEPRECATED("Use wrap_around_utils instead")
inline uint32_t LatestTimestamp(uint32_t timestamp1, uint32_t timestamp2) {
    return IsNewerTimestamp(timestamp1, timestamp2) ? timestamp1 : timestamp2;
}
    
} // namespace naivertc


#endif