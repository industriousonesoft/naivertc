#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_INTERFACE_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_INTERFACE_H_

#include "base/defines.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT BitrateEstimatorInterface {
public:
    virtual ~BitrateEstimatorInterface() = default;
    virtual void Update(Timestamp at_time, size_t amount, bool in_alr) = 0;

    virtual std::optional<DataRate> Estimate() const = 0;
    virtual std::optional<DataRate> PeekRate() const = 0;

    virtual void ExpectFastRateChange() = 0;
};
    
} // namespace naivertc


#endif