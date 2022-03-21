#ifndef _RTC_CONGESTION_CONTROL_PACING_BITRATE_PORBER_H_
#define _RTC_CONGESTION_CONTROL_PACING_BITRATE_PORBER_H_

#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_control/base/bwe_types.hpp"

#include <deque>

namespace naivertc {

// This class is used to manager the probe state and probe clusters.
class BitrateProber {
public:
    struct Configuration {
        // The minimum number probing packets used.
        int min_probe_packets_sent = 5;
        // A minimum interval between probes to allow
        // scheduling to be feasible.
        TimeDelta min_probe_delta = TimeDelta::Millis(1);
        TimeDelta min_probe_duration = TimeDelta::Millis(15);
        // The maximum amout of time each probe can
        // be delayed.
        TimeDelta max_probe_delay = TimeDelta::Millis(10);
        bool abort_delayed_probes = true;
    };
public:
    BitrateProber(const Configuration& config);
    ~BitrateProber();

    void SetEnabled(bool enabled);

    // Returns true if the prober is in a probing session, i.e., it currently
    // wants packets to be sent out according to the time returned by
    // TimeUntilNextProbe().
    bool IsProbing() const;

    // Initializes a new probing session if the prober is allowed to probe. Does
    // not initialize the prober unless the packet size is large enough to probe
    // with.
    void OnIncomingPacket(size_t packet_size);

    // Returns true if the prober is pushed back to the cluster queue.
    bool AddProbeCluster(int cluster_id, 
                         DataRate target_bitrate, 
                         Timestamp at_time);

    // Returns the time at which the next probe should be sent to get accurate
    // probing. If probing is not desired at this time, Timestamp::PlusInfinity()
    // will be returned.
    Timestamp NextTimeToProbe(Timestamp at_time) const;

    // Returns the next unexpired prober in cluster queue, 
    std::optional<ProbeCluster> CurrentProbeCluster(Timestamp at_time);

    // Returns the minimum number of bytes that the prober recommends for
    // the next probe, or zero if not probing.
    size_t RecommendedMinProbeSize() const;

    // Called to report to the prober that a probe has been sent. In case of
    // multiple packets per probe, this call would be made at the end of sending
    // the last packet in probe. |size| is the total size of all packets in probe.
    void OnProbeSent(size_t sent_bytes, Timestamp at_time);

private:
    struct ProbeClusterInfo;
    Timestamp CalculateNextProbeTime(const ProbeClusterInfo& cluster) const;

    inline bool IsProbeTimedOut(Timestamp at_time) const;

private:
    // ProbingState
    enum class ProbingState {
        // Probing will not be triggered in this state at all time.
        DISABLED,
        // Probing is enabled and ready to trigger on the first packet arrival.
        INACTIVE,
        // Probe cluster is filled with the set of bitrates to be probed and
        // probes are being sent.
        ACTIVE,
        // Probing is enabled, but currently suspended until an explicit trigger
        // to start probing again.
        SUSPENDED,
    };

    // A probe cluster consists of a set of probes. Each probe in turn can be
    // divided into a number of packets to accommodate the MTU on the network.
    struct ProbeClusterInfo {
        ProbeCluster probe_cluster;
        Timestamp created_at = Timestamp::MinusInfinity();
        Timestamp started_at = Timestamp::MinusInfinity();

        ProbeClusterInfo(ProbeCluster probe_cluster) 
            : probe_cluster(std::move(probe_cluster)) {};
    };

private:
    const Configuration config_;

    ProbingState probing_state_;

    // Probe bitrate per packet. These are used to compute the delta relative to
    // the previous probe packet based on the size and time when that packet was
    // sent.
    std::deque<ProbeClusterInfo> clusters_;

    // Time the next probe should be sent when in kActive state.
    Timestamp next_time_to_probe_;

    int total_probe_count_;
    int total_failed_probe_count_;

};
    
} // namespace naivertc

#endif