#include "rtc/congestion_controller/goog_cc/delay_based_bwe.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr TimeDelta kStreamTimeOut = TimeDelta::Seconds(2);
constexpr TimeDelta kSendTimeGroupLength = TimeDelta::Millis(5);

// CreateConfigOfRateControl
AimdRateControl::Configuration CreateConfigOfRateControl(bool send_side) {
    AimdRateControl::Configuration config;
    config.send_side = send_side;
    return config;
}
    
} // namespace

DelayBasedBwe::DelayBasedBwe(Configuration config) 
    : config_(std::move(config)),
      video_inter_arrival_delta_(nullptr),
      video_delay_detector_(new TrendlineEstimator({})),
      audio_inter_arrival_delta_(nullptr),
      audio_delay_detector_(new TrendlineEstimator({})),
      active_delay_detector_(video_delay_detector_.get()),
      last_seen_packet_(Timestamp::MinusInfinity()),
      last_video_packet_recv_time_(Timestamp::MinusInfinity()),
      audio_packets_since_last_video_(0),
      rate_control_(CreateConfigOfRateControl(/*send_side=*/true)),
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

DelayBasedBwe::Result DelayBasedBwe::IncomingPacketFeedbackVector(const TransportPacketsFeedback& packets_feedback_info,
                                                                  std::optional<DataRate> acked_bitrate,
                                                                  std::optional<DataRate> probe_bitrate,
                                                                  bool in_alr) {
    auto sorted_packet_feedbacks = packets_feedback_info.SortedByReceiveTime();
    // TODO: An empty feedback vector here likely means that
    // all acks were too late and that the send time history had
    // timed out. We should reduce the rate when this occurs.
    if (sorted_packet_feedbacks.empty()) {
        PLOG_WARNING << "Very late feedback received.";
        return Result();
    }

    bool delayed_feedback = true;
    bool recovered_from_overuse = false;
    BandwidthUsage prev_detector_state = active_delay_detector_->State();
    for (const auto& packet_feedback : sorted_packet_feedbacks) {
        delayed_feedback = false;
        IncomingPacketFeedback(packet_feedback, packets_feedback_info.feedback_time);
        if (prev_detector_state == BandwidthUsage::UNDERUSING &&
            active_delay_detector_->State() == BandwidthUsage::NORMAL) {
            recovered_from_overuse = true;
        }
        prev_detector_state = active_delay_detector_->State();
    }

    // FIXME: How to understan this mechanism? who not use sorted_packet_feedbacks.empty() instead?
    if (delayed_feedback) {
        // TODO(bugs.webrtc.org/10125): Design a better mechanism to safe-guard
        // against building very large network queues.
        return Result();
    }

    rate_control_.set_in_alr(in_alr);
    return MaybeUpdateEstimate(acked_bitrate, probe_bitrate, recovered_from_overuse, in_alr, packets_feedback_info.feedback_time);
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
void DelayBasedBwe::IncomingPacketFeedback(const PacketResult& packet_feedback, 
                                           Timestamp at_time) {
    // Reset if the stream has time out.
    if (last_seen_packet_.IsInfinite() ||
        at_time - last_seen_packet_ > kStreamTimeOut) {
        video_inter_arrival_delta_ = std::make_unique<InterArrivalDelta>(kSendTimeGroupLength);
        audio_inter_arrival_delta_ = std::make_unique<InterArrivalDelta>(kSendTimeGroupLength);

        video_delay_detector_.reset(new TrendlineEstimator({}));
        audio_delay_detector_.reset(new TrendlineEstimator({}));
        active_delay_detector_ = video_delay_detector_.get();
    }
    last_seen_packet_ = at_time;

    // As an alternative to ignoring small packets, we can separate audio and
    // video packets for overuse detection.
    TrendlineEstimator* delay_detector_for_packet = video_delay_detector_.get();
    if (config_.separate_audio) {
        if (packet_feedback.sent_packet.is_audio) {
            delay_detector_for_packet = audio_delay_detector_.get();
            audio_packets_since_last_video_++;
            if (audio_packets_since_last_video_ > config_.separate_packet_threshold &&
                packet_feedback.recv_time - last_video_packet_recv_time_ > config_.separate_time_threshold) {
                active_delay_detector_ = audio_delay_detector_.get();
            }
        } else {
            audio_packets_since_last_video_ = 0;
            last_video_packet_recv_time_ = std::max(last_video_packet_recv_time_, packet_feedback.recv_time);
            active_delay_detector_ = video_delay_detector_.get();
        }
    }
    size_t packet_size = packet_feedback.sent_packet.size;

    // FIXME: Why do we use the video inter arrival for the audio packets?
    InterArrivalDelta* inter_arrival_for_packet = (config_.separate_audio && packet_feedback.sent_packet.is_audio) 
                                                  ? video_inter_arrival_delta_.get()
                                                  : audio_inter_arrival_delta_.get();
    // Waits for two adjacent packet group, and try to compute the deltas of them.
    auto deltas = inter_arrival_for_packet->ComputeDeltas(packet_feedback.sent_packet.send_time,
                                                          packet_feedback.recv_time,
                                                          at_time,
                                                          packet_size);
    // Two adjacent packet groups have arrived.
    if (deltas) {
        delay_detector_for_packet->Update(deltas->arrival_time_delta.ms(),
                                          deltas->send_time_delta.ms(),
                                          packet_feedback.sent_packet.send_time.ms(),
                                          packet_feedback.recv_time.ms(),
                                          packet_size);
    }
    
}

DelayBasedBwe::Result DelayBasedBwe::MaybeUpdateEstimate(std::optional<DataRate> acked_bitrate,
                                                         std::optional<DataRate> probe_bitrate,
                                                         bool recovered_from_overuse,
                                                         bool in_alr,
                                                         Timestamp at_time) {
    Result ret;
    // Currently overusing the bandwidth.
    if (active_delay_detector_->State() == BandwidthUsage::OVERUSING) {
        if (has_once_detected_overuse_ && in_alr && alr_limited_backoff_enabled_) {
            if (rate_control_.TimeToReduceFurther(at_time, prev_bitrate_)) {
                auto [target_bitrate, updated] = UpdateEstimate(at_time, prev_bitrate_);
                ret.updated = updated;
                ret.target_bitrate = target_bitrate;
                ret.backoff_in_alr = true;
            }
        } else if (acked_bitrate && rate_control_.TimeToReduceFurther(at_time, *acked_bitrate)) {
            auto [target_bitrate, updated] = UpdateEstimate(at_time, *acked_bitrate);
            ret.updated = updated;
            ret.target_bitrate = target_bitrate;
        } else if (!acked_bitrate && rate_control_.ValidEstimate() &&
                   rate_control_.InitialTimeToReduceFurther(at_time)) {
            // Overusing before we have a measured acknowledged bitrate.
            // Reduce send rate by 50% every 200 ms.
            // TODO: Improve this and/or the acknowledged bitrate estimator
            // so that we (almost) always have a bitrate estimate.
            rate_control_.SetEstimate(rate_control_.LatestEstimate() / 2, at_time);
            ret.updated = true;
            ret.probe = false;
            ret.target_bitrate = rate_control_.LatestEstimate();
        }
        has_once_detected_overuse_ = true;
    } else {
        if (probe_bitrate) {
            ret.probe = true;
            ret.updated = true;
            ret.target_bitrate = *probe_bitrate;
            rate_control_.SetEstimate(*probe_bitrate, at_time);
        } else {
            auto [target_bitrate, updated] = UpdateEstimate(at_time, acked_bitrate);
            ret.updated = updated;
            ret.target_bitrate = target_bitrate;
            ret.recovered_from_overuse = recovered_from_overuse;
        }
    }
    BandwidthUsage detector_state = active_delay_detector_->State();
    if ((ret.updated && prev_bitrate_ != ret.target_bitrate) ||
        detector_state != prev_state_) {
        DataRate bitrate = ret.updated ? ret.target_bitrate : prev_bitrate_;
        prev_bitrate_ = bitrate;
        prev_state_ = detector_state;
    }

    return ret;
}

std::pair<DataRate, bool> DelayBasedBwe::UpdateEstimate(Timestamp at_time, 
                                                        std::optional<DataRate> acked_bitrate) {
    auto target_bitrate = rate_control_.Update(active_delay_detector_->State(), acked_bitrate, at_time);
    return {target_bitrate, rate_control_.ValidEstimate()};
}
    
} // namespace naivertc
