#include "rtc/base/ntp_time.hpp"

namespace naivertc {
namespace rtcp {

NtpTime::NtpTime() : value_(0) {}

NtpTime::NtpTime(uint64_t value) : value_(value) {}

NtpTime::NtpTime(uint32_t seconds, uint32_t fractions) : value_(seconds * kFractionsPerSecond + fractions) {}

void NtpTime::Set(uint32_t seconds, uint32_t fractions) {
    value_ = seconds * kFractionsPerSecond + fractions;
}

void NtpTime::Reset() {
    value_ = 0;
}

int64_t NtpTime::ToMs() const {
    static constexpr double kNtpFracPerMs = 4.294967296E6;  // 2^32 / 1000
    const double frac_ms = static_cast<double>(fractions()) / kNtpFracPerMs;
    return static_cast<int64_t>(seconds()) * 1000 + static_cast<int64_t>(frac_ms + 0.5);
}

uint32_t NtpTime::seconds() const {
    return utils::numeric::checked_static_cast<uint32_t>(value_ / kFractionsPerSecond);
}

uint32_t NtpTime::fractions() const {
    return utils::numeric::checked_static_cast<uint32_t>(value_ % kFractionsPerSecond);
}
    
} // namespace rtcp    
} // namespace naivertc
