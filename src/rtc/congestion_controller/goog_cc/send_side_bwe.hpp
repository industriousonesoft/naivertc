#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_SEND_SIDE_BWE_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_SEND_SIDE_BWE_H_

#include "base/defines.hpp"
#include "rtc/congestion_controller/goog_cc/rtt_based_backoff.hpp"
#include "rtc/congestion_controller/goog_cc/linker_capacity_tracker.hpp"
#include "rtc/congestion_controller/goog_cc/loss_based_bwe.hpp"

#include <deque>

namespace naivertc {

class RTC_CPP_EXPORT SendSideBwe {
public:
    SendSideBwe();
    ~SendSideBwe();

private:
    // User Metrics Analysis
    enum UmaState { NO_UPDATE, FIRST_DONE, DONE };

private:
    RttBasedBackoff rtt_backoff_;
    LinkerCapacityTracker linker_capacity_tracker_;

    std::deque<std::pair<Timestamp, DataRate>> min_bitrate_history_;

    // Incoming filter
    int lost_packtes_since_last_loss_update_;
    int expected_packets_since_last_loss_update_;

    DataRate curr_target_bitrate_;
    DataRate min_configured_bitrate_;
    DataRate max_configured_bitrate_;

    bool has_decreased_since_last_fraction_loss_;
    Timestamp time_last_loss_feedback_;
    Timestamp time_last_loss_packet_report_;
    uint8_t last_fraction_loss_;
    TimeDelta last_rtt_;

    // The max bitrate () as set by the receiver.
    // This is typically signalled using the REMB (Receiver Estimated Maximum Bitrate) message
    // and is used when we don't have any send side delay based estimate.
    DataRate remb_limit_;
    bool remb_limit_cpas_only_;
    DataRate delay_based_limit_;
    Timestamp time_last_decrease_;
    Timestamp time_first_report_;
    int initially_loss_packets_;
    DataRate bitrate_at_2_seconds_;
    UmaState uma_update_state_;
    UmaState uam_rtt_state_;
    std::vector<bool> rampup_uma_states_updated_;
    float low_loss_threshold_;
    float high_loss_threshold_;
    DataRate bitrate_threshold_;
    LossBasedBwe loss_based_bwe_;
};
    
} // namespace naivertc


#endif