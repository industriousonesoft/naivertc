#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_BWE_DEFINES_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_BWE_DEFINES_H_

#include "base/defines.hpp"
#include "rtc/base/units/data_rate.hpp"

#include <iostream>

namespace naivertc {

constexpr DataRate kDefaultMinBitrate = DataRate::BitsPerSec(5'000); // 5kbps
constexpr DataRate kDefaultMaxBitrate = DataRate::BitsPerSec(1000'000'000); // 1000mbps

enum class BandwidthUsage {
    NORMAL,
    UNDERUSING,
    OVERUSING
};

enum class RateControlState { 
    HOLD, 
    INCREASE, 
    DECREASE 
};

std::ostream& operator<<(std::ostream& out, BandwidthUsage usage);
    
} // namespace naivertc


#endif