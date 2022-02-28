#ifndef _RTC_CONGESTION_CONTROLLER_BASE_RATE_CONTROL_SETTING_H_
#define _RTC_CONGESTION_CONTROLLER_BASE_RATE_CONTROL_SETTING_H_

#include "rtc/congestion_controller/base/bwe_defines.hpp"

#include <optional>

namespace naivertc {

// CongestionWindwoConfiguration
struct CongestionWindwoConfiguration {
    std::optional<TimeDelta> queuing_delay = kDefaultAcceptedQueuingDelay;
    std::optional<DataRate> min_pushback_bitrate = kDefaultMinPushbackTargetBitrate;
    bool drop_frame_only = true;
    bool probe_on_max_allocation_changed = true;

    bool IsEnabled() const;
    // When pushback is enabled, the pacer is oblivious to the congestion window.
    // The relation between outstanding data and the congestion window will affects
    // encoder allocations directly.
    bool IsPushbackEnabled() const;
};
    
} // namespace naivertc


#endif