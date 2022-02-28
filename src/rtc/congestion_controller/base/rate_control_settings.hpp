#ifndef _RTC_CONGESTION_CONTROLLER_BASE_RATE_CONTROL_SETTING_H_
#define _RTC_CONGESTION_CONTROLLER_BASE_RATE_CONTROL_SETTING_H_

#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/time_delta.hpp"

#include <optional>

namespace naivertc {

// CongestionWindwoConfiguration
struct CongestionWindwoConfiguration {
    std::optional<TimeDelta> queue_addtional_time = TimeDelta::Millis(300);
    std::optional<DataRate> min_pushback_bitrate = DataRate::BitsPerSec(30'000);
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