#include "rtc/congestion_control/send_side/goog_cc/delay_based/delay_based_bwe.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr TimeDelta kStreamTimeOut = TimeDelta::Seconds(2);
constexpr TimeDelta kSendTimeGroupLength = TimeDelta::Millis(5);
    
} // namespace

DelayBasedBwe::DelayBasedBwe(Configuration config) 
    : separate_audio_(std::move(config.separate_audio_config)),
      video_inter_arrival_delta_(nullptr),
      video_delay_detector_(new TrendlineEstimator(std::move(config.video_trendline_estimator_config))),
      audio_inter_arrival_delta_(nullptr),
      audio_delay_detector_(new TrendlineEstimator(std::move(config.audio_trendline_estimator_config))),
      active_delay_detector_(video_delay_detector_.get()),
      last_feedback_arrival_time_(Timestamp::MinusInfinity()),
      last_video_packet_arrival_time_(Timestamp::MinusInfinity()),
      audio_packets_since_last_video_(0),
      rate_control_(std::move(config.aimd_rate_control_config), true),
      prev_bitrate_(DataRate::Zero()),
      has_once_detected_overuse_(false),
      prev_state_(BandwidthUsage::NORMAL),
      alr_limited_backoff_enabled_(false) {}

DelayBasedBwe::~DelayBasedBwe() = default;

void DelayBasedBwe::set_alr_limited_backoff_enabled(bool enbaled) {
    alr_limited_backoff_enabled_ = enbaled;
}

void DelayBasedBwe::OnRttUpdate(TimeDelta avg_rtt) {
    rate_control_.set_rtt(avg_rtt);
}

void DelayBasedBwe::SetStartBitrate(DataRate start_bitrate) {
    PLOG_INFO << "Setting start bitrate to " << start_bitrate.bps() << " bps.";
    rate_control_.SetStartBitrate(start_bitrate);
}

void DelayBasedBwe::SetMinBitrate(DataRate min_bitrate) {
    PLOG_INFO << "Setting min bitrate to " << min_bitrate.bps() << " bps.";
    rate_control_.SetMinBitrate(min_bitrate);
}

DelayBasedBwe::Result DelayBasedBwe::IncomingPacketFeedbacks(const TransportPacketsFeedback& feedback_report,
                                                             std::optional<DataRate> acked_bitrate,
                                                             std::optional<DataRate> probe_bitrate,
                                                             bool in_alr) {
    auto packet_feedbacks = feedback_report.SortedByReceiveTime();
    // NOTE: NetworkTransportStatistician中发送的包到接收反馈的时间窗口为1分钟。
    // 换言之，某个包的反馈是在其发送1分钟后才收到则会被丢弃掉，因此有可能导致packet_feedbacks为空。
    // 详见NetworkTransportStatistician::AddPacket
    // TODO(bugs.webrtc.org/10125): Design a better mechanism to safe-guard
    // against building very large network queues.
    if (packet_feedbacks.empty()) {
        PLOG_WARNING << "Very late feedback received.";
        return Result();
    }

    bool recovered_from_underuse = false;
    BandwidthUsage prev_state = active_delay_detector_->State();
    for (const auto& packet_feedback : packet_feedbacks) {
        // Detect the current bandwidth usage.
        auto curr_state = Detect(packet_feedback, feedback_report.receive_time);
        if (prev_state == BandwidthUsage::UNDERUSING &&
            curr_state == BandwidthUsage::NORMAL) {
            recovered_from_underuse = true;
        }
        prev_state = curr_state;
    }

    // Do not increase the delay-based estimate in alr.
    rate_control_.set_in_alr(in_alr);
    return MaybeUpdateEstimate(acked_bitrate, probe_bitrate, recovered_from_underuse, in_alr, feedback_report.receive_time);
}

std::pair<DataRate, bool> DelayBasedBwe::LatestEstimate() const {
    return {rate_control_.LatestEstimate(), rate_control_.ValidEstimate()}; 
}

TimeDelta DelayBasedBwe::GetExpectedBwePeriod() const {
    return rate_control_.GetExpectedBandwidthPeriod();
}

DataRate DelayBasedBwe::TriggerOveruse(Timestamp at_time, std::optional<DataRate> link_capacity) {
    return rate_control_.Update(BandwidthUsage::OVERUSING, link_capacity, at_time);
}

DataRate DelayBasedBwe::last_estimate() const {
    return prev_bitrate_;
}

// Private methods
BandwidthUsage DelayBasedBwe::Detect(const PacketResult& packet_feedback, 
                                     Timestamp at_time) {
    // Reset if the stream has time out.
    if (last_feedback_arrival_time_.IsInfinite() ||
        at_time - last_feedback_arrival_time_ > kStreamTimeOut) {
        video_inter_arrival_delta_ = std::make_unique<InterArrivalDelta>(kSendTimeGroupLength);
        audio_inter_arrival_delta_ = std::make_unique<InterArrivalDelta>(kSendTimeGroupLength);

        video_delay_detector_.reset(new TrendlineEstimator({}));
        audio_delay_detector_.reset(new TrendlineEstimator({}));
        active_delay_detector_ = video_delay_detector_.get();
    }
    last_feedback_arrival_time_ = at_time;

    // As an alternative to ignoring small packets, we can separate audio and
    // video packets for overuse detection.
    TrendlineEstimator* delay_detector_for_packet = video_delay_detector_.get();
    if (separate_audio_.enabled) {
        if (packet_feedback.sent_packet.is_audio) {
            delay_detector_for_packet = audio_delay_detector_.get();
            audio_packets_since_last_video_++;
            // The conditions to separate audio and video packet for overuse detection:
            // 1. The audio packets have accumulated since the last video packet arrived are more than `packet_threshold`;
            // 2. The time has elapsed since the last video packet arrived are more the `time_threshold`.
            if (audio_packets_since_last_video_ > separate_audio_.packet_threshold &&
                packet_feedback.recv_time - last_video_packet_arrival_time_ > separate_audio_.time_threshold) {
                active_delay_detector_ = audio_delay_detector_.get();
            }
        } else {
            audio_packets_since_last_video_ = 0;
            last_video_packet_arrival_time_ = std::max(last_video_packet_arrival_time_, packet_feedback.recv_time);
            active_delay_detector_ = video_delay_detector_.get();
        }
    }
    size_t packet_size = packet_feedback.sent_packet.size;

    // Choose the |inter_arrival| correspond to the incoming pakcet.
    InterArrivalDelta* inter_arrival_for_packet = (separate_audio_.enabled && 
                                                   packet_feedback.sent_packet.is_audio) 
                                                   ? audio_inter_arrival_delta_.get()
                                                   : video_inter_arrival_delta_.get();
    // Waits for two adjacent packet group arriving, and try to compute the deltas of them.
    auto deltas = inter_arrival_for_packet->ComputeDeltas(packet_feedback.sent_packet.send_time,
                                                          packet_feedback.recv_time,
                                                          at_time,
                                                          packet_size);                                     
    // Detected two adjacent packet groups.
    if (deltas) {
        PLOG_VERBOSE_IF(false) << "inter-departure=" << deltas->send_time_delta.ms()
                               << " - inter-arrval=" << deltas->arrival_time_delta.ms()
                               << std::endl;
        delay_detector_for_packet->Update(deltas->arrival_time_delta.ms(),
                                          deltas->send_time_delta.ms(),
                                          packet_feedback.sent_packet.send_time.ms(),
                                          packet_feedback.recv_time.ms(),
                                          packet_size);
    }
    return active_delay_detector_->State();
}

DelayBasedBwe::Result DelayBasedBwe::MaybeUpdateEstimate(std::optional<DataRate> acked_bitrate,
                                                         std::optional<DataRate> probe_bitrate,
                                                         bool recovered_from_underuse,
                                                         bool in_alr,
                                                         Timestamp at_time) {
    PLOG_VERBOSE_IF(false) << "acked_bitrate=" << acked_bitrate.value_or(DataRate::Zero()).bps()
                           << " bps - probe_bitrate=" << probe_bitrate.value_or(DataRate::Zero()).bps()
                           << " bps - in_alr: " << (in_alr ? "true" : "false")
                           << " - recovered_from_underuse: " << (recovered_from_underuse ? "true" : "false");
    Result ret;
    BandwidthUsage detected_state = active_delay_detector_->State();
    // Currently overusing the bandwidth.
    if (detected_state == BandwidthUsage::OVERUSING) {
        if (has_once_detected_overuse_ && in_alr && alr_limited_backoff_enabled_) {
            // Check if we can reduce the current bitrate further to close to `prev_bitrate_`.
            if (rate_control_.CanReduceFurther(at_time, prev_bitrate_)) {
                auto [target_bitrate, updated] = UpdateEstimate(BandwidthUsage::OVERUSING, prev_bitrate_, at_time);
                ret.updated = updated;
                ret.target_bitrate = target_bitrate;
                ret.backoff_in_alr = true;
            }
        // Check if we can reduce the current bitrate further to close to `acked_bitrate`.
        } else if (acked_bitrate && rate_control_.CanReduceFurther(at_time, *acked_bitrate)) {
            auto [target_bitrate, updated] = UpdateEstimate(BandwidthUsage::OVERUSING, *acked_bitrate, at_time);
            ret.updated = updated;
            ret.target_bitrate = target_bitrate;
        // Reduce the curren bitrate further if overusing before we have measured a throughout (in start phase).
        } else if (!acked_bitrate && rate_control_.ValidEstimate() &&
                   rate_control_.CanReduceFurtherInStartPhase(at_time)) {
            // Overusing before we have a measured acknowledged bitrate.
            // Reduce send rate by 50% every rtt [10ms, 200 ms].
            // TODO: Improve this and/or the acknowledged bitrate estimator
            // so that we (almost) always have a bitrate estimate.
            rate_control_.SetEstimate(rate_control_.LatestEstimate() / 2, at_time);
            ret.updated = true;
            ret.probe = false;
            ret.target_bitrate = rate_control_.LatestEstimate();
        }
        has_once_detected_overuse_ = true;
    } else {
        // In the HOLD or DECREASE state.
        // The probed bitrate has a high priority.
        if (probe_bitrate) {
            ret.probe = true;
            ret.updated = true;
            ret.target_bitrate = *probe_bitrate;
            rate_control_.SetEstimate(*probe_bitrate, at_time);
        } else {
            // Retrieve the current bitrate from AIMD rate control.
            auto [target_bitrate, updated] = UpdateEstimate(detected_state, acked_bitrate, at_time);
            ret.updated = updated;
            ret.target_bitrate = target_bitrate;
            ret.recovered_from_underuse = recovered_from_underuse;
        }
    }
    
    if ((ret.updated && prev_bitrate_ != ret.target_bitrate) ||
        detected_state != prev_state_) {
        auto curr_bitrate = ret.updated ? ret.target_bitrate : prev_bitrate_;
        PLOG_VERBOSE << "state: " << prev_state_ << " => " << detected_state
                     << "- bitrate: " << prev_bitrate_.kbps<double>() 
                     << " kbps => " << curr_bitrate.kbps<double>() 
                     << " kbps - is probed: " << (ret.probe ? "true" : "false")
                     << "at_time: " << at_time.ms()
                     << std::endl;
        prev_bitrate_ = curr_bitrate;
        prev_state_ = detected_state;
    }

    return ret;
}

std::pair<DataRate, bool> DelayBasedBwe::UpdateEstimate(BandwidthUsage bw_state, 
                                                        std::optional<DataRate> acked_bitrate,
                                                        Timestamp at_time) {
    auto target_bitrate = rate_control_.Update(bw_state, acked_bitrate, at_time);
    return {target_bitrate, rate_control_.ValidEstimate()};
}
    
} // namespace naivertc
