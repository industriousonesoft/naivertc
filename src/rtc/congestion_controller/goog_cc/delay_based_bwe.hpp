#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_H_

#include "base/defines.hpp"

namespace naivertc {

// A bandwidth estimation based on delay.
class RTC_CPP_EXPORT DelayBasedBwe {
public:
    DelayBasedBwe() = delete;
    DelayBasedBwe(const DelayBasedBwe&) = delete;
    DelayBasedBwe& operator=(const DelayBasedBwe&) = delete;
    ~DelayBasedBwe();
};
    
} // namespace naivertc


#endif