#ifndef _RTC_BASE_NTP_TIME_H_
#define _RTC_BASE_NTP_TIME_H_

#include "base/defines.hpp"
#include "common/utils.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace naivertc {
namespace rtcp {

class RTC_CPP_EXPORT NtpTime {
public:
    static constexpr uint64_t kFractionsPerSecond = 0x100000000; // 2^32

    NtpTime();
    explicit NtpTime(uint64_t value);
    NtpTime(uint32_t seconds, uint32_t fractions);

    NtpTime(const NtpTime&) = default;
    NtpTime& operator=(const NtpTime&) = default;
    explicit operator uint64_t() const { return value_; }

    // NTP standard (RFC1305, section 3.1) explicitly state value 0 is invalid.
    bool Valid() const  { return value_ != 0; }

    void Set(uint32_t seconds, uint32_t fractions);
    void Reset();

    int64_t ToMs() const;
    
    uint32_t seconds() const;
    uint32_t fractions() const;

private:
    uint64_t value_;
};

inline bool operator==(const NtpTime& n1, const NtpTime& n2) {
    return static_cast<uint64_t>(n1) == static_cast<uint64_t>(n2);
}

inline bool operator!=(const NtpTime& n1, const NtpTime& n2) {
    return !(n1 == n2);
}

// Convert 'int64_t' milliseconds to Q32.32-formattted fixed-point seconds.
// Performs clamping if the result overflows or underflows
inline int64_t Int64MsToQ32x32(int64_t milliseconds) {
    double result = std::round(milliseconds * (NtpTime::kFractionsPerSecond / 1000.0));

    // Explicitily cast value double to avoid implicit conversion warnings
    // The conversion of the std::numeric_limits<int64_t>::max() triggers
    // -Wimplicit-int-float-conversion warning in clang 10.0.0 without exlicit cast.
    if (result <= static_cast<double>(std::numeric_limits<int64_t>::min())) {
        return std::numeric_limits<int64_t>::min();
    }

    if (result >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
        return std::numeric_limits<int64_t>::max();
    }

    return utils::numeric::checked_static_cast<int64_t>(result);
}

// Convert 'int64_t' milliseconds to UQ32.32-formattted fixed-point seconds.
// Performs clamping if the result overflows or underflows
inline uint64_t Int64MsToUQ32x32(int64_t milliseconds) {
    double result = std::round(milliseconds * (NtpTime::kFractionsPerSecond / 1000.0));

    // Explicitily cast value double to avoid implicit conversion warnings
    // The conversion of the std::numeric_limits<int64_t>::max() triggers
    // -Wimplicit-int-float-conversion warning in clang 10.0.0 without exlicit cast.
    if (result <= static_cast<double>(std::numeric_limits<uint64_t>::min())) {
        return std::numeric_limits<uint64_t>::min();
    }

    if (result >= static_cast<double>(std::numeric_limits<uint64_t>::max())) {
        return std::numeric_limits<uint64_t>::max();
    }

    return utils::numeric::checked_static_cast<uint64_t>(result);
}

inline int64_t Q32x32ToInt64Ms(int64_t q32x32) {
    return utils::numeric::checked_static_cast<int64_t>(std::round(q32x32 * (1000.0 / NtpTime::kFractionsPerSecond)));
}

inline int64_t Q32x32ToInt64Ms(uint64_t q32x32) {
    return utils::numeric::checked_static_cast<int64_t>(std::round(q32x32 * (1000.0 / NtpTime::kFractionsPerSecond)));
}
    
} // namespace rtcp
} // namespace naivertc

#endif