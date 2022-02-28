#include "rtc/congestion_controller/base/rate_control_settings.hpp"

namespace naivertc {

bool CongestionWindwoConfiguration::IsEnabled() const {
    return queuing_delay.has_value();
}

bool CongestionWindwoConfiguration::IsPushbackEnabled() const {
    return IsEnabled() && min_pushback_bitrate.has_value();
}
    
} // namespace naivertc
