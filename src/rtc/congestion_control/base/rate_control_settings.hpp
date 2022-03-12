#ifndef _RTC_CONGESTION_CONTROL_BASE_RATE_CONTROL_SETTING_H_
#define _RTC_CONGESTION_CONTROL_BASE_RATE_CONTROL_SETTING_H_

#include "rtc/congestion_control/base/bwe_defines.hpp"

#include <optional>

namespace naivertc {

// RateControlSettings
struct RateControlSettings {
    // Congestion window settings
    std::optional<TimeDelta> queuing_delay = kDefaultAcceptedQueuingDelay;
    std::optional<DataRate> min_pushback_bitrate = kDefaultMinPushbackTargetBitrate;
    std::optional<size_t> initial_congestion_window = 0;
    bool drop_frame_only = true;

    // Probe settings
    bool probe_on_max_allocation_changed = true;

    bool UseCongestionWindow() const;
    // When pushback is enabled, the pacer is oblivious to the congestion window.
    // The relation between outstanding data and the congestion window will affects
    // encoder allocations directly.
    bool UseCongestionWindowPushback() const;
};
    
} // namespace naivertc


#endif