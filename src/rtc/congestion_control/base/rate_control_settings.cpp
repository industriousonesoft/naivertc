#include "rtc/congestion_control/base/rate_control_settings.hpp"

namespace naivertc {

bool RateControlSettings::UseCongestionWindow() const {
    return queuing_delay.has_value();
}

bool RateControlSettings::UseCongestionWindowPushback() const {
    return UseCongestionWindow() && min_pushback_bitrate.has_value();
}
    
} // namespace naivertc
