#include "rtc/congestion_controller/goog_cc/goog_cc_network_controller.hpp"
#include "rtc/congestion_controller/goog_cc/bitrate_estimator.hpp"

#include <plog/Log.h>

#include <vector>
#include <numeric>

namespace naivertc {
namespace {

// From RTCPSender video report interval.
constexpr TimeDelta kLossUpdateInterval = TimeDelta::Millis(1000);

// Pacing-rate relative to our target send rate.
// Multiplicative factor that is applied to the target bitrate to calculate
// the number of bytes that can be transmitted per interval.
// Increasing this factor will result in lower delays in cases of bitrate
// overshoots from the encoder.
constexpr float kDefaultPaceMultiplier = 2.5f;

// If the probe result is far below the current throughput estimate
// it's unlikely that the probe is accurate, so we don't want to drop too far.
// However, if we actually are overusing, we want to drop to something slightly
// below the current throughput estimate to drain the network queues.
constexpr double kProbeDropThroughputFraction = 0.85;
    
} // namespace


GoogCcNetworkController::GoogCcNetworkController(Configuration config) 
    : packet_feedback_only_(false),
      use_min_allocated_bitrate_as_lower_bound_(false),
      ignore_probes_lower_than_network_estimate_(false),
      limit_probes_lower_than_throughput_estimate_(false),
      loss_based_stable_bitrate_(false),
      send_side_bwe_(std::make_unique<SendSideBwe>(SendSideBwe::Configuration())),
      delay_based_bwe_(std::make_unique<DelayBasedBwe>(DelayBasedBwe::Configuration())),
      ack_bitrate_estimator_(AcknowledgedBitrateEstimator::Create(BitrateEstimator::Configuration())),
      probe_bitrate_estimator_(std::make_unique<ProbeBitrateEstimator>()),
      last_loss_based_target_bitrate_(config.constraints.starting_bitrate.value_or(DataRate::Zero())),
      last_stable_target_bitrate_(last_loss_based_target_bitrate_),
      pacing_factor_(config.stream_based_config.pacing_factor.value_or(kDefaultPaceMultiplier)),
      max_padding_bitrate_(config.stream_based_config.allocated_bitrate_limits.max_padding_bitrate),
      min_total_allocated_bitrate_(config.stream_based_config.allocated_bitrate_limits.min_total_allocated_bitrate),
      max_total_allocated_bitrate_(config.stream_based_config.allocated_bitrate_limits.max_total_allocated_bitrate),
      initial_config_(std::move(config)) {
    if (delay_based_bwe_) {
        delay_based_bwe_->SetMinBitrate(kMinBitrate);
    }
}

GoogCcNetworkController::~GoogCcNetworkController() {}

NetworkControlUpdate GoogCcNetworkController::OnNetworkAvailability(NetworkAvailability) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnNetworkRouteChange(NetworkRouteChange) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnProcessInterval(ProcessInterval) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnRemoteBitrateReport(RemoteBitrateReport report) {
    if (packet_feedback_only_) {
        PLOG_ERROR << "Received REMB for packet feedback only GoogCC.";
        return NetworkControlUpdate();
    }
    send_side_bwe_->OnRemb(report.bitrate, report.receive_time);
    return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnRoundTripTimeUpdate(RoundTripTimeUpdate msg) {
    if (packet_feedback_only_ || msg.smoothed) {
        return NetworkControlUpdate();
    }
    if (delay_based_bwe_) {
        delay_based_bwe_->OnRttUpdate(msg.rtt);
    }
    send_side_bwe_->OnRtt(msg.rtt, msg.receive_time);
    return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnSentPacket(SentPacket sent_packet) {
    if (!first_packet_sent_) {
        first_packet_sent_ = true;
        // Initialize feedback time to send time to allow estimation of RTT until
        // first feedback is received.
        send_side_bwe_->OnPropagationRtt(TimeDelta::Zero(), sent_packet.send_time);
    }
    send_side_bwe_->OnSentPacket(sent_packet);
    return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnReceivedPacket(ReceivedPacket received_packet) {
    last_packet_received_time_ = received_packet.receive_time;
    return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnStreamsConfig(StreamsConfig msg) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnTargetBitrateConstraints(TargetBitrateConstraints) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnTransportLossReport(TransportLossReport) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnTransportPacketsFeedback(TransportPacketsFeedback report) {
    if (report.packet_feedbacks.empty()) {
        return NetworkControlUpdate();
    }
    TimeDelta max_feedback_rtt = TimeDelta::MinusInfinity();
    TimeDelta min_propagation_rtt = TimeDelta::PlusInfinity();
    Timestamp max_recv_time = Timestamp::MinusInfinity();

    std::vector<PacketResult> feedbacks = report.ReceivedWithSendInfo();
    for (const auto& feedback : feedbacks) {
        max_recv_time = std::max(max_recv_time, feedback.recv_time);
    }

    for (const auto& feedback : feedbacks) {
        TimeDelta feedback_rtt = report.feedback_time - feedback.sent_packet.send_time;
        TimeDelta min_pending_time = feedback.recv_time - max_recv_time;
        TimeDelta propagation_rtt = feedback_rtt - min_pending_time;
        max_feedback_rtt = std::max(max_feedback_rtt, feedback_rtt);
        min_propagation_rtt = std::min(min_propagation_rtt, propagation_rtt);
    }

    if (max_feedback_rtt.IsFinite()) {
        feedback_max_rtts_.push_back(max_feedback_rtt.ms());
        const size_t kMaxFeedbackRttWindow = 32;
        if (feedback_max_rtts_.size() > kMaxFeedbackRttWindow) {
            feedback_max_rtts_.pop_front();
            send_side_bwe_->OnPropagationRtt(min_propagation_rtt, report.feedback_time);
        }
    }

    if (packet_feedback_only_) {
        if (!feedback_max_rtts_.empty()) {
            // SMA: Simple Moving Average.
            int64_t sum_rtt_ms = std::accumulate(feedback_max_rtts_.begin(), feedback_max_rtts_.end(), 0);
            int64_t mean_rtt_ms = sum_rtt_ms / feedback_max_rtts_.size();
            if (delay_based_bwe_) {
                delay_based_bwe_->OnRttUpdate(TimeDelta::Millis(mean_rtt_ms));
            }
        }

        TimeDelta feedback_min_rtt = TimeDelta::PlusInfinity();
        for (const auto& packet_feedback : feedbacks) {
            TimeDelta pending_time = packet_feedback.recv_time - max_recv_time;
            TimeDelta rtt = report.feedback_time - packet_feedback.sent_packet.send_time - pending_time;
            // Value used for predicting NACK round trip time in FEC
            feedback_min_rtt = std::min(rtt, feedback_min_rtt);
        }
        if (feedback_min_rtt.IsFinite()) {
            send_side_bwe_->OnRtt(feedback_min_rtt, report.feedback_time);
        }

        // Loss information
        expected_packets_since_last_loss_update_ += report.PacketsWithFeedback().size();
        for (const auto& packet_feedback : report.PacketsWithFeedback()) {
            if (packet_feedback.IsLost()) {
                lost_packets_since_last_loss_update_ += 1;
            }
        }
        if (report.feedback_time > time_next_loss_update_) {
            time_next_loss_update_ = report.feedback_time + kLossUpdateInterval;
            send_side_bwe_->OnPacketsLost(lost_packets_since_last_loss_update_, expected_packets_since_last_loss_update_, report.feedback_time);
            expected_packets_since_last_loss_update_ = 0;
            lost_packets_since_last_loss_update_ = 0;
        }
    }

    auto feeback_packets = report.SortedByReceiveTime();
    ack_bitrate_estimator_->IncomingPacketFeedbacks(feeback_packets);
    auto ack_bitrate = ack_bitrate_estimator_->Estimate();
    send_side_bwe_->OnAcknowledgeBitrate(ack_bitrate, report.feedback_time);

    send_side_bwe_->IncomingPacketFeedbacks(report);
    for (const auto& feedback : feeback_packets) {
        if (feedback.sent_packet.pacing_info.probe_cluster) {
            probe_bitrate_estimator_->IncomingProbePacketFeedback(feedback);
        }
    }

    auto probe_bitrate = probe_bitrate_estimator_->Estimate();
    if (limit_probes_lower_than_throughput_estimate_ && probe_bitrate && ack_bitrate) {
        // Limit the backoff to slightly below the acknowledged bitrate, because we want 
        // to drain the queues if we are actually overusing.
        auto backoffed_ack_bitrate = ack_bitrate.value() * kProbeDropThroughputFraction;
        // The acknowledged bitrate shouldn't normally be higher than the delay based estimate,
        // but it could happen (e.g. due to packet bursts or encoder overshoot.). we use
        // std::min to ensure a probe bitrate below the current BWE never causes an increase.
        DataRate curr_bwe = std::min(delay_based_bwe_->last_estimate(), backoffed_ack_bitrate);
        // If the probe bitrate lower then the current BWE, using the current BWE instead.
        // Since the probe bitrate has a higher priority than the acknowledged bitrate (in
        // delay_based_bwe) in non-overusing state.
        probe_bitrate = std::max(*probe_bitrate, curr_bwe);
    }

    NetworkControlUpdate update;
    auto result = delay_based_bwe_->IncomingPacketFeedbacks(report,
                                                            ack_bitrate,
                                                            probe_bitrate,
                                                            false);
    if (result.updated) {
        if (result.probe) {
            send_side_bwe_->OnSendBitrate(result.target_bitrate, report.feedback_time);
        }
        send_side_bwe_->OnDelayBasedBitrate(result.target_bitrate, report.feedback_time);
        MaybeTriggerOnNetworkChanged(&update, report.feedback_time);
    }

    
    if (result.recovered_from_overuse) {

    } else if (result.backoff_in_alr) {

    }
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnNetworkStateEstimate(NetworkEstimate) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::GetNetworkState(Timestamp at_time) const {
    NetworkControlUpdate update;
    update.target_rate = TargetTransferRate();
    update.target_rate->network_estimate.at_time = at_time;
    update.target_rate->network_estimate.loss_rate_ratio = last_estimated_fraction_loss_.value_or(0) / 255.0;
    update.target_rate->network_estimate.rtt = last_estimated_rtt;
    update.target_rate->network_estimate.bwe_period = delay_based_bwe_->GetExpectedBwePeriod();

    update.target_rate->at_time = at_time;
    // update.target_rate->target_bitrate
    // Using the estimated link capacity as the stable target bitrate.
    update.target_rate->stable_target_bitrate = send_side_bwe_->EstimatedLinkCapacity();

    // update.pacer_config = 
    update.congestion_window = curr_data_window_;
    return update;
}

// Private methods
void GoogCcNetworkController::MaybeTriggerOnNetworkChanged(NetworkControlUpdate* update, Timestamp at_time) {

}
    
} // namespace naivertc
