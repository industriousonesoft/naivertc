#ifndef _RTC_CONGESTION_CONTROL_PACING_BITRATE_PORBER_H_
#define _RTC_CONGESTION_CONTROL_PACING_BITRATE_PORBER_H_

#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"

namespace naivertc {

class BitrateProber {
public:
    struct Configuration {
        // The minimum number probing packets used.
        int min_probe_packets_sent;
        // A minimum interval between probes to allow
        // scheduling to be feasible.
        TimeDelta min_probe_delta;
        TimeDelta min_probe_duration;
        // The maximum amout of time each probe can
        // be delayed.
        TimeDelta max_probe_delay;
        bool abort_delayed_probes;
    };
public:
    BitrateProber(Configuration config);
    ~BitrateProber();

private:
    const Configuration config_;
};
    
} // namespace naivertc

#endif