#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_RTT_BASED_BWE_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_RTT_BASED_BWE_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_controller/network_types.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RttBasedBackoff {
public:
    RttBasedBackoff();
    ~RttBasedBackoff();

    void Update(TimeDelta rtt,
                Timestamp at_time);

    void OnSentPacket(const SentPacket& sent_packet);

    // Returns the RTT with backoff.
    TimeDelta CorrectedRtt() const;

private:
    TimeDelta last_rtt_;
    Timestamp time_last_rtt_update_;
    Timestamp time_last_packet_sent_;
};
    
} // namespace naivertc


#endif