#ifndef _RTC_CONGESTION_CONTROL_PACING_TYPES_H_
#define _RTC_CONGESTION_CONTROL_PACING_TYPES_H_

#include "rtc/base/units/data_rate.hpp"

#include <optional>

namespace naivertc {

// ProbeCluster
struct ProbeCluster {
    int id = -1;
    // The mininum probes of a cluster need to do at least.
    int min_probes = 0;
    // The mininum bytes of a cluster need to send at least.
    size_t min_bytes = 0;
    // The bitrate that is supposed to probe.
    DataRate target_bitrate = DataRate::Zero();
    // The actual probes has sent.
    int sent_probes = 0;
    // The actual bytes has sent.
    size_t sent_bytes = 0;

    ProbeCluster(int id, 
                 int min_probes, 
                 size_t min_bytes, 
                 DataRate target_bitrate);

    bool IsDone() const;
};

// PacedPacketInfo
struct PacedPacketInfo {
    std::optional<ProbeCluster> probe_cluster = std::nullopt;
};

} // namespace naivertc 

#endif