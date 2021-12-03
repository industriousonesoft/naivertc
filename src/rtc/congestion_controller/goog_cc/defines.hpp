#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_DEFINES_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_DEFINES_H_

#include "base/defines.hpp"

namespace naivertc {

enum class BandwidthUsage {
    NORMAL,
    UNDERUSING,
    OVERUSING
};
    
} // namespace naivertc


#endif