#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_SEND_SIDE_BWE_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_SEND_SIDE_BWE_H_

#include "base/defines.hpp"
#include "rtc/congestion_controller/components/rtt_based_backoff.hpp"
#include "rtc/congestion_controller/goog_cc/linker_capacity_tracker.hpp"
#include "rtc/congestion_controller/goog_cc/loss_feedback_based_bwe.hpp"
#include "rtc/congestion_controller/goog_cc/loss_report_based_bwe.hpp"

#include <deque>
#include <optional>

namespace naivertc {

class SendSideBwe {
public:
    struct Configuration {
        bool enable_loss_feedback_based_control = true;
        TimeDelta rtt_limit = TimeDelta::Seconds(3);
        double drop_factor = 0.8;
        TimeDelta drop_interval = TimeDelta::Seconds(1);
        DataRate bandwidth_floor = DataRate::KilobitsPerSec(5);
    };
public:
    SendSideBwe(Configuration config);
    ~SendSideBwe();

    DataRate target_bitate() const;
    DataRate min_bitate() const;
    uint8_t fraction_loss() const;
    TimeDelta rtt() const;

    DataRate EstimatedLinkCapacity() const;

    void OnBitrates(std::optional<DataRate> send_bitrate,
                    DataRate min_bitrate,
                    DataRate max_bitrate,
                    Timestamp report_time);

    void OnSendBitrate(DataRate bitrate,
                       Timestamp report_time);

    void OnDelayBasedBitrate(DataRate bitrate,
                             Timestamp report_time);

    void OnAcknowledgedBitrate(std::optional<DataRate> ack_bitrate,
                              Timestamp report_time);

    void OnPropagationRtt(TimeDelta rtt,
                          Timestamp report_time);

    void OnSentPacket(const SentPacket& sent_packet);

    // Call when we receive a RTCP message with TMMBR or REMB.
    void OnRemb(DataRate bitrate,
                Timestamp report_time);

    // Call when we receive a RTCP message with a RecieveBlock.
    void OnPacketsLostReport(int64_t num_packets_lost,
                             int64_t num_packets,
                             Timestamp report_time);

    // Call when we receive a RTCP message with a ReceiveBlock.   
    void OnRtt(TimeDelta rtt,
               Timestamp report_time);

    void IncomingPacketFeedbacks(const TransportPacketsFeedback& report);

    void UpdateEstimate(Timestamp report_time);

    void SetMinMaxBitrate(DataRate min_bitrate,
                          DataRate max_bitrate);

private:
    DataRate Clamp(DataRate bitrate) const;
    void ApplyLimits(Timestamp report_time);
    void UpdateTargetBitrate(DataRate bitrate, 
                             Timestamp report_time);

    bool IsInStartPhase(Timestamp report_time) const;

    void UpdateMinHistory(DataRate bitrate, Timestamp report_time);

private:
    const Configuration config_;

    DataRate curr_bitrate_;
    DataRate min_configured_bitrate_;
    DataRate max_configured_bitrate_;
    std::optional<DataRate> ack_bitrate_;

    TimeDelta last_rtt_;

    // The max bitrate () as set by the receiver.
    // This is typically signalled using the REMB (Receiver Estimated Maximum Bitrate) message
    // and is used when we don't have any send side delay based estimate.
    DataRate remb_limit_;
    bool use_remb_limit_cpas_only_;
    DataRate delay_based_limit_;
    Timestamp time_first_report_;
    Timestamp time_last_decrease_;

    std::deque<std::pair<Timestamp, DataRate>> min_bitrate_history_;

    RttBasedBackoff rtt_backoff_;
    LinkerCapacityTracker linker_capacity_tracker_;
  
    std::optional<LossFeedbackBasedBwe> loss_feedback_based_bwe_;
    LossReportBasedBwe loss_report_based_bwe_;
};
    
} // namespace naivertc


#endif