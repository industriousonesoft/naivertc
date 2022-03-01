#ifndef _RTC_CONGESTION_CONTROL_GOOG_CC_AIMD_RATE_CONTROL_H_
#define _RTC_CONGESTION_CONTROL_GOOG_CC_AIMD_RATE_CONTROL_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/congestion_control/base/bwe_defines.hpp"
#include "rtc/congestion_control/controllers/goog_cc/delay_based/link_capacity_estimator.hpp"

#include <optional>

namespace naivertc {

// A rate control implementation based on AIMD (additive increases of bitrate
// when no over-use is detected and multiplicative decreases when over-uses
// are detected).
// When we think the available bandwidth has changes or is unknown, we will
// switch to 'slow-start mode' to increase bitrate multiplicatively.
class AimdRateControl {
public:
    struct Configuration {
        // Indicates if we use a adaptive threhold instead a fixed one
        // to detecte the bandwidth usage in OveruseDetector.
        bool adaptive_threshold_enabled = true;
        bool no_bitrate_increase_in_alr = false;
        bool link_capacity_fix = false;
        std::optional<TimeDelta> initial_backoff_interval = std::nullopt;
        DataRate min_bitrate = DataRate::BitsPerSec(5'000);
        DataRate max_bitrate = DataRate::KilobitsPerSec(30'000);
    };
public:
    AimdRateControl(Configuration config, bool send_side);
    ~AimdRateControl();

    void set_rtt(TimeDelta rtt);
    void set_in_alr(bool in_alr);
    void SetStartBitrate(DataRate start_bitrate);
    void SetMinBitrate(DataRate min_bitrate);
    void SetEstimate(DataRate bitrate, Timestamp at_time);
    bool ValidEstimate() const;
    TimeDelta GetFeedbackInterval() const;

    DataRate LatestEstimate() const;

    // Returns true if the bitrate estimate hasn't been changed for more than
    // an RTT, or if the estimated_throughput is less than half of the current
    // estimate. Should be used to decide if we should reduce the rate further
    // when over-using.
    bool CanReduceFurther(Timestamp at_time, DataRate estimated_throughput) const;

    // As above. To be used if overusing before we have measured a throughput (in start phase).
    bool CanReduceFurtherInStartPhase(Timestamp at_time) const;

    DataRate Update(BandwidthUsage bw_state, 
                    std::optional<DataRate> estimated_throughput, 
                    Timestamp at_time);

    // Returns the increase rate per second when used bandwidth is near the link capacity.
    DataRate GetNearMaxIncreaseRatePerSecond() const;

    // Returns the expected time between overuse signals (assuming steady state).
    TimeDelta GetExpectedBandwidthPeriod() const;

private:
    DataRate ClampBitrate(DataRate new_bitrate) const;
    DataRate MultiplicativeRateIncrease(Timestamp at_time, 
                                        Timestamp last_time, 
                                        DataRate curr_bitrate) const;
    DataRate AdditiveRateIncrease(Timestamp at_time, 
                                  Timestamp last_time) const;
    
    void ChangeBitrate(BandwidthUsage bw_state, 
                       std::optional<DataRate> estimated_throughput, 
                       Timestamp at_time);
    void ChangeState(BandwidthUsage bw_state, 
                     Timestamp at_time);
    
    bool DontIncreaseInAlr() const;

    bool TimeToReduceFurther(Timestamp at_time) const;
    bool SufficientToReduceFurther(DataRate estimated_throughput) const;

private:
    const Configuration config_;
    const bool send_side_;
    DataRate min_configured_bitrate_;
    DataRate curr_bitrate_;
    DataRate latest_estimated_throughput_;
    
    LinkCapacityEstimator link_capacity_;

    RateControlState rate_control_state_;
    Timestamp time_last_bitrate_change_;
    Timestamp time_last_bitrate_decrease_;
    Timestamp time_first_throughput_arrive_;
    bool is_bitrate_initialized_;
    double backoff_factor_;
    // ALR (Application Limited Region)
    bool in_alr_;
    TimeDelta rtt_;
    std::optional<DataRate> last_decreased_bitrate_;
};
    
} // namespace naivert 

#endif