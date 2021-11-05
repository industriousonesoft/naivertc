#include "rtc/base/time/ntp_time_util.hpp"
#include "rtc/base/divide_round.hpp"
#include "common/utils_time.hpp"

namespace naivertc {

uint32_t SaturatedUsToCompactNtp(int64_t us) {
    constexpr uint32_t kMaxCompactNtp = 0xFFFFFFFF;
    constexpr int kCompactNtpInSecond = 0x10000;
    if (us <= 0)
        return 0;
    if (us >= kMaxCompactNtp * kNumMicrosecsPerSec / kCompactNtpInSecond)
        return kMaxCompactNtp;
    // To convert to compact ntp need to divide by 1e6 to get seconds,
    // then multiply by 0x10000 to get the final result.
    // To avoid float operations, multiplication and division swapped.
    return DivideRoundToNearest(us * kCompactNtpInSecond, kNumMicrosecsPerSec);
}

int64_t CompactNtpRttToMs(uint32_t compact_ntp_interval) {
    // Interval to convert expected to be positive, e.g. rtt or delay.
    // Because interval can be derived from non-monotonic ntp clock,
    // it might become negative that is indistinguishable from very large values.
    // Since very large rtt/delay are less likely than non-monotonic ntp clock,
    // those values consider to be negative and convert to minimum value of 1ms.
    if (compact_ntp_interval > 0x80000000)
        return 1;
    // Convert to 64bit value to avoid multiplication overflow.
    int64_t value = static_cast<int64_t>(compact_ntp_interval);
    // To convert to milliseconds need to divide by 2^16 to get seconds,
    // then multiply by 1000 to get milliseconds. To avoid float operations,
    // multiplication and division swapped.
    int64_t ms = DivideRoundToNearest(value * 1000, 1 << 16);
    // Rtt value 0 considered too good to be true and increases to 1.
    return std::max<int64_t>(ms, 1);
}
    
} // namespace naivertc
