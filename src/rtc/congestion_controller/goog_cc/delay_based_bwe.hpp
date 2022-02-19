#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/congestion_controller/goog_cc/inter_arrival_delta.hpp"
#include "rtc/congestion_controller/goog_cc/trendline_estimator.hpp"
#include "rtc/congestion_controller/goog_cc/aimd_rate_control.hpp"
#include "rtc/congestion_controller/network_types.hpp"

namespace naivertc {

// A bandwidth estimation based on delay.
class DelayBasedBwe {
public:
    using AimdRateControlConfig = AimdRateControl::Configuration;
    using TrendlineEstimatorConfig = TrendlineEstimator::Configuration;
    struct SeparateAudioConfiguration {
        // Denotes whether to separate audio from video packets to detect bandwidth usage.
        bool enabled = false;
        // The audio packets have accumulated until the next video packet arrive.
        int packet_threshold = 10;
        // The time has elpased until the next video packet arrive.
        TimeDelta time_threshold = TimeDelta::Seconds(1);
    };

    // TODO: Implement the field trial to set the hyper parameters below.
    struct Configuration {
        AimdRateControlConfig aimd_rate_control_config;
        TrendlineEstimatorConfig audio_trendline_estimator_config;
        TrendlineEstimatorConfig video_trendline_estimator_config;
        SeparateAudioConfiguration separate_audio_config;
    };

    struct Result {
        bool updated = false;
        bool probe = false;
        DataRate target_bitrate = DataRate::Zero();
        bool recovered_from_overuse = false;
        bool backoff_in_alr = false;
    };
public:
    explicit DelayBasedBwe(Configuration config);
    DelayBasedBwe() = delete;
    DelayBasedBwe(const DelayBasedBwe&) = delete;
    DelayBasedBwe& operator=(const DelayBasedBwe&) = delete;
    ~DelayBasedBwe();

    void set_alr_limited_backoff_enabled(bool enbaled);

    void OnRttUpdate(TimeDelta avg_rtt);
    void SetStartBitrate(DataRate start_bitrate);
    void SetMinBitrate(DataRate min_bitrate);

    Result IncomingPacketFeedbacks(const TransportPacketsFeedback& packets_feedback_info,
                                   std::optional<DataRate> acked_bitrate,
                                   std::optional<DataRate> probe_bitrate,
                                   bool in_alr);

    // Return a pair consisting of the lastest estimated bitrate
    // and a bool denoting whether a valid estimate exists.
    std::pair<DataRate, bool> LatestEstimate() const;
    TimeDelta GetExpectedBwePeriod() const;
    DataRate TriggerOveruse(Timestamp at_time, std::optional<DataRate> link_capacity);
    DataRate last_estimate() const;

private:
    // Try to detect the current bandwidth usage with the incoming packet feedback.
    BandwidthUsage Detect(const PacketResult& packet_feedback, 
                          Timestamp at_time);

    Result MaybeUpdateEstimate(std::optional<DataRate> acked_bitrate,
                               std::optional<DataRate> probe_bitrate,
                               bool recovered_from_overuse,
                               bool in_alr,
                               Timestamp at_time);

    // Updates the current remote rate estimate.
    // Return a pair consisting of a updated bitrate process by AIMD 
    // and a bool denoting whether a valid estimate exists.
    std::pair<DataRate, bool> UpdateEstimate(BandwidthUsage bw_state, 
                                             std::optional<DataRate> acked_bitrate,
                                             Timestamp at_time);

private:
    const SeparateAudioConfiguration separate_audio_;

    std::unique_ptr<InterArrivalDelta> video_inter_arrival_delta_;
    std::unique_ptr<TrendlineEstimator> video_delay_detector_;
    std::unique_ptr<InterArrivalDelta> audio_inter_arrival_delta_;
    std::unique_ptr<TrendlineEstimator> audio_delay_detector_;
    TrendlineEstimator* active_delay_detector_;

    Timestamp last_feedback_arrival_time_;
    Timestamp last_video_packet_arrival_time_;
    int64_t audio_packets_since_last_video_;
    AimdRateControl rate_control_;
    DataRate prev_bitrate_;
    bool has_once_detected_overuse_;
    BandwidthUsage prev_state_;
    bool alr_limited_backoff_enabled_;
};
    
} // namespace naivertc


#endif