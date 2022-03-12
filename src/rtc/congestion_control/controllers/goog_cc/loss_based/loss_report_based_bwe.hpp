#ifndef _RTC_CONGESTION_CONTROL_GOOG_CC_LOSS_REPORT_BASED_BWE_H_
#define _RTC_CONGESTION_CONTROL_GOOG_CC_LOSS_REPORT_BASED_BWE_H_

#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_control/base/bwe_defines.hpp"

namespace naivertc {

class LossReportBasedBwe {
public:
    struct Configuration {
        float low_loss_threshold = 0.02f; // 2%
        float high_loss_threshold = 0.1f; // 10%
        DataRate max_bitrate_threshold = DataRate::Zero();
    };
public:
    LossReportBasedBwe(Configuration config);
    ~LossReportBasedBwe();

    // Return loss ratio in Q8 format.
    uint8_t fraction_loss() const;

    void OnPacketsLostReport(int64_t num_packets_lost,
                             int64_t num_packets,
                             Timestamp report_time);

    std::pair<DataRate, RateControlState> Estimate(DataRate min_bitrate,
                                                   DataRate expected_birate,
                                                   TimeDelta rtt,
                                                   Timestamp at_time);

private:
    bool IsLossReportExpired(Timestamp at_time) const;

private:
    const Configuration config_;
    // The fraction part of loss ratio in Q8 format.
    uint8_t fraction_loss_;
    // The number of lost packets has accumuted since the last loss report.
    int accumulated_lost_packets_;
    // The number of packets has accumulated since the last loss report.
    int accumulated_packets_;

    bool has_decreased_since_last_fraction_loss_;
    Timestamp time_last_fraction_loss_update_;
    Timestamp time_last_decrease_;
};
    
} // namespace naivertc


#endif