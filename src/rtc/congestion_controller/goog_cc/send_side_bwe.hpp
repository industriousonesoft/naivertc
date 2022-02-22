#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_SEND_SIDE_BWE_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_SEND_SIDE_BWE_H_

#include "base/defines.hpp"
#include "rtc/congestion_controller/components/linker_capacity_tracker.hpp"
#include "rtc/congestion_controller/components/rtt_based_backoff.hpp"
#include "rtc/congestion_controller/goog_cc/loss_based_bwe.hpp"

#include <deque>
#include <optional>

namespace naivertc {

class SendSideBwe {
public:
    struct Configuration {
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

    void OnAcknowledgeBitrate(std::optional<DataRate> ack_bitrate,
                              Timestamp report_time);

    void OnPropagationRtt(TimeDelta rtt,
                          Timestamp report_time);

    void OnSentPacket(const SentPacket& sent_packet);

    // Call when we receive a RTCP message with TMMBR or REMB.
    void OnRemb(DataRate bitrate,
                Timestamp report_time);

    // Call when we receive a RTCP message with a RecieveBlock.
    void OnPacketsLost(int64_t num_packets_lost,
                       int64_t num_packets,
                       Timestamp report_time);

    // Call when we receive a RTCP message with a ReceiveBlock.   
    void OnRtt(TimeDelta rtt,
               Timestamp report_time);

    void IncomingPacketFeedbacks(const TransportPacketsFeedback& report);

    void SetBitrateBoundary(DataRate min_bitrate,
                            DataRate max_bitrate);

    void UpdateEstimate(Timestamp report_time);

private:
    // User Metrics Analysis
    enum UmaState { NO_UPDATE, FIRST_DONE, DONE };

    DataRate Clamp(DataRate bitrate) const;
    void ApplyLimits(Timestamp report_time);
    void UpdateTargetBitrate(DataRate bitrate, 
                             Timestamp report_time);

    bool IsInStartPhase(Timestamp report_time) const;

    void UpdateMinHistory(DataRate bitrate, Timestamp report_time);

    void UpdateUmaStats(int packet_lost, Timestamp report_time);

private:
    const Configuration config_;

    RttBasedBackoff rtt_backoff_;
    LinkerCapacityTracker linker_capacity_tracker_;

    std::deque<std::pair<Timestamp, DataRate>> min_bitrate_history_;

    // The number of lost packets has accumuted since the last loss report.
    int accumulated_lost_packets_;
    // The number of packets has accumulated since the last loss report.
    int accumulated_packets_;

    DataRate curr_bitrate_;
    DataRate min_configured_bitrate_;
    DataRate max_configured_bitrate_;
    std::optional<DataRate> ack_bitrate_;

    bool has_decreased_since_last_fraction_loss_;
    Timestamp time_last_fraction_loss_update_;
    // The fraction part of loss ratio in Q8 format.
    uint8_t last_fraction_loss_;
    TimeDelta last_rtt_;

    // The max bitrate () as set by the receiver.
    // This is typically signalled using the REMB (Receiver Estimated Maximum Bitrate) message
    // and is used when we don't have any send side delay based estimate.
    DataRate remb_limit_;
    bool use_remb_limit_cpas_only_;
    DataRate delay_based_limit_;
    Timestamp time_last_decrease_;
    Timestamp time_first_report_;
    int initially_loss_packets_;
    DataRate bitrate_at_start_;
    UmaState uma_update_state_;
    UmaState uma_rtt_state_;
    std::vector<bool> rampup_uma_states_updated_;
    float low_loss_threshold_;
    float high_loss_threshold_;
    DataRate bitrate_threshold_;

    std::optional<LossBasedBwe> loss_based_bwe_;
};
    
} // namespace naivertc


#endif