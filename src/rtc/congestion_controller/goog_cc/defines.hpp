#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_DEFINES_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_DEFINES_H_

#include "base/defines.hpp"

#include <iostream>

namespace naivertc {

constexpr int kMinBitrateBps = 5000;

enum class BandwidthUsage {
    NORMAL,
    UNDERUSING,
    OVERUSING
};

std::ostream& operator<<(std::ostream& out, BandwidthUsage usage);
    
} // namespace naivertc


#endif