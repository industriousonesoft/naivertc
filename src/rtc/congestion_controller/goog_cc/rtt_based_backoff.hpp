#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_RTT_BASED_BACKOFF_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_RTT_BASED_BACKOFF_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_controller/network_types.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RttBasedBackoff {
public:
    struct Configuration {
        TimeDelta rtt_limit = TimeDelta::Seconds(3);
        double drop_fraction = 0.8;
        TimeDelta drop_interval = TimeDelta::Seconds(1);
        DataRate bandwidth_floor = DataRate::KilobitsPerSec(5);
    };

public:
    RttBasedBackoff(Configuration config);
    ~RttBasedBackoff();

    void Update(TimeDelta rtt,
                Timestamp at_time);

    void OnSentPacket(const SendPacket& sent_packet);

    TimeDelta CorrectedRtt() const;

private:
    const Configuration config_;
    TimeDelta last_rtt_;
    Timestamp time_last_rtt_update_;
    Timestamp time_last_packet_sent_;
};
    
} // namespace naivertc


#endif