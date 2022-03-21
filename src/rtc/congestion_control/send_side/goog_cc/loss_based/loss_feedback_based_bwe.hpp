#ifndef _RTC_CONGESTION_CONTORLLER_GOOG_CC_LOSS_FEEDBACK_BASED_BWE_H_
#define _RTC_CONGESTION_CONTORLLER_GOOG_CC_LOSS_FEEDBACK_BASED_BWE_H_

#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_control/base/bwe_types.hpp"
#include "rtc/congestion_control/base/bwe_defines.hpp"

#include <optional>

namespace naivertc {

// This class estimates an upper BWE limit based on loss feedbacks,
// and require transport feedback and acknowledged bitrate.
class LossFeedbackBasedBwe {
public:
    struct Configuration {
        double min_increase_factor = 1.02;
        double max_increase_factor = 1.08;
        TimeDelta increase_low_rtt = TimeDelta::Millis(200);
        TimeDelta increase_high_rtt = TimeDelta::Millis(800);
        double decrease_factor = 0.99;
        TimeDelta loss_window = TimeDelta::Millis(800);
        TimeDelta loss_max_window = TimeDelta::Millis(800);
        TimeDelta ack_rate_max_window = TimeDelta::Millis(800);
        DataRate increase_offset = DataRate::BitsPerSec(1000);
        DataRate loss_bandwidth_balance_increase = DataRate::KilobitsPerSec(0.5);
        DataRate loss_bandwidth_balance_decrease = DataRate::KilobitsPerSec(4);
        DataRate loss_bandwidth_balance_reset = DataRate::KilobitsPerSec(0.1);
        double loss_bandwidth_balance_exponent = 0.5;
        bool allow_resets = false;
        TimeDelta decrease_interval = TimeDelta::Millis(300);
        TimeDelta loss_report_timeout = TimeDelta::Millis(6000);
    };
public:
    LossFeedbackBasedBwe(Configuration config);
    ~LossFeedbackBasedBwe();

    bool InUse() const;

    void SetInitialBitrate(DataRate bitrate);

    void OnPacketFeedbacks(const std::vector<PacketResult>& packet_feedbacks, 
                           Timestamp at_time);
    void OnAcknowledgedBitrate(DataRate acked_bitrate, 
                               Timestamp at_time);

     std::pair<DataRate, RateControlState> Estimate(DataRate min_bitrate,
                                                    DataRate expected_birate,
                                                    TimeDelta rtt,
                                                    Timestamp at_time);

private:
    // The threshold of loss ratio to reset bitrate.
    double ThresholdToReset() const;
    // The threshold of loss ratio to increase bitrate.
    double ThresholdToIncrease() const;
    // The threshold of loss ratio to decrease bitrate.
    double ThresholdToDecrease() const;
private:
    const Configuration config_;
    double avg_loss_ratio_;
    double avg_loss_ratio_max_;
    double last_loss_ratio_;
    bool has_decreased_since_last_loss_report_;
    DataRate loss_based_bitrate_;
    DataRate acked_bitrate_max_;
    Timestamp time_acked_bitrate_last_update_;
    Timestamp time_last_decrease_;
    Timestamp time_last_loss_packet_report_;    
};
    
} // namespace naivertc


#endif