#ifndef _RTC_CONGESTION_GOOG_CC_CONTROLLER_ALR_DETECTOR_H_
#define _RTC_CONGESTION_GOOG_CC_CONTROLLER_ALR_DETECTOR_H_

#include "rtc/congestion_control/components/interval_budget.hpp"
#include "rtc/base/units/timestamp.hpp"

#include <optional>

namespace naivertc {

class Clock;

// Application limited region detector
// This is a helper class that utilizes signals of elapsed time and
// bytes sent to estimate whether network traffic is currently limited
// by the application's ability to generate traffic.
//
// AlrDetector provides a signal that can be utilized to adjust
// estimate bandwidth.
class AlrDetector {
public:
    struct Configuration {
        // The bandwidth used to increase the ALR budget.
        double bandwidth_usage_ratio = 0.65;
        // Indicates a new ALR starts when bandwidth usage is below 20%. 
        double start_budget_level_ratio = 0.8;
        // Indicates a new ALR ends when bandwidth usage is above 50%. 
        double stop_budget_level_ratio = 0.5;
    };
public:
    AlrDetector(Configuration config, Clock* clock);
    ~AlrDetector();

    std::optional<Timestamp> alr_started_time() const { return alr_started_time_; }
    std::optional<Timestamp> alr_ended_time() const { return alr_ended_time_; }

    bool InAlr() const { return alr_started_time_.has_value(); }

    void SetTargetBitrate(DataRate bitrate);

    void OnBytesSent(size_t bytes_sent, Timestamp send_time);
    
private:
    const Configuration config_;
    Clock* const clock_;

    IntervalBudget alr_budget_;
    
    std::optional<Timestamp> last_send_time_;
    std::optional<Timestamp> alr_started_time_;
    std::optional<Timestamp> alr_ended_time_;
};
    
} // namespace naivertc

#endif