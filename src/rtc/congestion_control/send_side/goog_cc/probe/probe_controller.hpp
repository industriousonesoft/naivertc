#ifndef _RTC_CONGESTION_CONTROL_GOOG_CC_PROBE_CONTROLLER_H_
#define _RTC_CONGESTION_CONTROL_GOOG_CC_PROBE_CONTROLLER_H_

#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/congestion_control/base/bwe_types.hpp"

#include <vector>
#include <optional>

namespace naivertc {

// This class controls initiation of probing to estimate initial channel
// capacity. There is also support for probing during a session when max
// bitrate is adjusted by an application.
class ProbeController {
public:
    struct Configuration {
        double first_exponential_probe_scale = 3.0;
        double second_exponential_probe_scale = 6.0;
        double further_exponential_probe_scale = 2;
        double further_probe_scale= 0.7;

        TimeDelta alr_probing_interval = TimeDelta::Seconds(5);
        double alr_probe_scale = 2;

        double first_allocation_probe_scale = 1;
        double second_allocation_probe_scale = 2;
        bool allocation_allow_further_probing = false;
        DataRate allocation_probe_cap = DataRate::PlusInfinity();

        // Indicates if probing is limited by the max allocated bitrate.
        bool limit_probes_with_allocatable_bitrate = true;
    };

public:
    ProbeController(const Configuration& config);
    ~ProbeController();

    void set_enable_periodic_alr_probing(bool enable);

    void set_alr_start_time(std::optional<Timestamp> start_time);
    void set_alr_end_time(Timestamp end_time);

    std::vector<ProbeClusterConfig> OnBitrates(DataRate start_bitrate,
                                               DataRate min_bitrate,
                                               DataRate max_bitrate,
                                               Timestamp at_time);
    std::vector<ProbeClusterConfig> OnMaxTotalAllocatedBitrate(DataRate max_total_allocated_bitrate,
                                                               Timestamp at_time);
    std::vector<ProbeClusterConfig> OnNetworkAvailability(NetworkAvailability msg);                        

    std::vector<ProbeClusterConfig> OnEstimatedBitrate(DataRate estimate,
                                                       Timestamp at_time);

    std::vector<ProbeClusterConfig> OnPeriodicProcess(Timestamp at_time);

    std::vector<ProbeClusterConfig> RequestProbe(Timestamp at_time);

    void Reset(Timestamp at_time);                                         
                        
private:
    std::vector<ProbeClusterConfig> InitProbing(std::vector<DataRate> bitrates_to_probe,
                                                bool probe_further, 
                                                Timestamp at_time);
    std::vector<ProbeClusterConfig> InitExponentialProbing(Timestamp at_time);

    bool InAlr() const;

private:
    enum class ProbingState {
        // No probing has been triggered yet.
        NEW,
        // Waiting for probing result to continue further probing.
        WAITING,
        // Porbing is complete.
        DONE,
    };

    struct MidCallProbing {
        ProbingState probing_state = ProbingState::NEW;
        DataRate bitrate_to_probe = DataRate::Zero();
        // A threshold to indicate if the probing is successful or not.
        DataRate success_threshold = DataRate::MinusInfinity();
    };
private:
    const Configuration config_;
    bool enable_periodic_alr_probing_ = false;
    bool network_available_ = true;
    ProbingState probing_state_ = ProbingState::NEW;

    DataRate start_bitrate_ = DataRate::Zero();
    DataRate estimated_bitrate_ = DataRate::Zero();
    DataRate max_bitrate_ = DataRate::Zero();
    DataRate max_total_allocated_bitrate_ = DataRate::Zero();

    Timestamp time_last_probing_initiated_ = Timestamp::Zero();
    Timestamp time_last_large_drop_ = Timestamp::Zero();
    Timestamp time_last_probe_request_ = Timestamp::Zero();

    DataRate bitrate_before_last_large_drop_ = DataRate::Zero();

    std::optional<DataRate> min_bitrate_to_probe_further_ = std::nullopt;

    std::optional<MidCallProbing> mid_call_probing_ = std::nullopt;

    int32_t next_probe_cluster_id_ = 1;

    std::optional<Timestamp> alr_start_time_ = std::nullopt;
    std::optional<Timestamp> alr_end_time_ = std::nullopt;

};
    
} // namespace naivertc


#endif