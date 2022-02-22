#ifndef _RTC_CONGESTION_GOOG_CC_CONTROLLER_ALR_DETECTOR_H_
#define _RTC_CONGESTION_GOOG_CC_CONTROLLER_ALR_DETECTOR_H_

#include "rtc/congestion_controller/components/interval_budget.hpp"
#include "rtc/base/units/timestamp.hpp"

#include <optional>

namespace naivertc {

class Clock;

// This is a helper class that utilizes signals of elapsed time and
// bytes sent to estimate whether network traffic is currently limited
// by the application's ability to generate traffic.
//
// AlrDetector provides a signal that can be utilized to adjust
// estimate bandwidth.
class AlrDetector {
public:
    struct Configuration {
        double bandwidth_usage_ratio = 0.65;
        double start_budget_level_ratio = 0.8;
        double stop_budget_level_ratio = 0.5;
    };
public:
    AlrDetector(Configuration config, Clock* clock);
    ~AlrDetector();

    std::optional<Timestamp> alr_started_time() const { return alr_started_time_; }
    std::optional<Timestamp> alr_ended_time() const { return alr_ended_time_; }

    bool InAlr() const { return alr_started_time_.has_value(); }

    void OnByteSent(size_t bytes_sent, Timestamp send_time);
    void OnEstimate(DataRate bitrate);

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