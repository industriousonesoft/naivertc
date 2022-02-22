#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_LINKER_CAPACITY_TRACKER_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_LINKER_CAPACITY_TRACKER_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"

#include <optional>

namespace naivertc {

class LinkerCapacityTracker {
public:
    LinkerCapacityTracker(TimeDelta tracking_window = TimeDelta::Seconds(10));
    ~LinkerCapacityTracker();

    void OnStartingBitrate(DataRate bitrate);
    void OnDelayBasedEstimate(DataRate bitrate, 
                              Timestamp at_time);
    // Call when the estimated bitrate has been dropped
    // since congestion has detected by the RTT estimate (with backoff).
    void OnRttBasedEstimate(DataRate bitrate,
                            Timestamp at_time);
    void Update(DataRate bitrate, 
                Timestamp at_time);

    DataRate estimate() const;

private:
    TimeDelta tracking_window_;
    DataRate estimated_capacity_;
    DataRate last_delay_based_estimate_;
    Timestamp time_last_capacity_udpate_;
};
    
} // namespace naivertc


#endif