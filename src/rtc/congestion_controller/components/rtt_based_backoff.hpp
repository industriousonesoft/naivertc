#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_RTT_BASED_BWE_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_RTT_BASED_BWE_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_controller/base/network_types.hpp"

namespace naivertc {

class RttBasedBackoff {
public:
    RttBasedBackoff();
    ~RttBasedBackoff();

    void OnSentPacket(Timestamp at_time);
    void OnPropagationRtt(TimeDelta rtt,
                          Timestamp at_time);

    // Returns the RTT with backoff.
    TimeDelta CorrectedRtt(Timestamp at_time) const;

private:
    TimeDelta last_rtt_;
    Timestamp time_last_rtt_update_;
    Timestamp time_last_packet_sent_;
};
    
} // namespace naivertc


#endif