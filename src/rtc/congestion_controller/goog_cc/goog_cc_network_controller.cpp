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

constexpr size_t kMaxFeedbackRttWindow = 32;
    
} // namespace


GoogCcNetworkController::GoogCcNetworkController(Configuration config) 
    : packet_feedback_only_(false),
      use_min_allocated_bitrate_as_lower_bound_(false),
      ignore_probes_lower_than_network_estimate_(false),
      limit_probes_lower_than_throughput_estimate_(false),
      loss_based_stable_bitrate_(false),
      send_side_bwe_(std::make_unique<SendSideBwe>(SendSideBwe::Configuration())),
      delay_based_bwe_(std::make_unique<DelayBasedBwe>(DelayBasedBwe::Configuration())),
      acknowledged_bitrate_estimator_(AcknowledgedBitrateEstimator::Create(BitrateEstimator::Configuration())),
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

NetworkControlUpdate GoogCcNetworkController::OnRemoteBitrateUpdated(DataRate bitrate, Timestamp receive_time) {
    if (packet_feedback_only_) {
        PLOG_ERROR << "Received REMB for packet feedback only GoogCC.";
        return NetworkControlUpdate();
    }
    send_side_bwe_->OnRemb(bitrate, receive_time);
    return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnRttUpdated(TimeDelta rtt, Timestamp receive_time) {
    if (packet_feedback_only_) {
        return NetworkControlUpdate();
    }
    if (delay_based_bwe_) {
        delay_based_bwe_->OnRttUpdate(rtt);
    }
    send_side_bwe_->OnRtt(rtt, receive_time);
    return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnSentPacket(const SentPacket& sent_packet) {
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

NetworkControlUpdate GoogCcNetworkController::OnTransportLostReport(const TransportLossReport& loss_report) {
    if (packet_feedback_only_) {
        return NetworkControlUpdate();
    }
    send_side_bwe_->OnPacketsLost(loss_report.num_packets_lost, loss_report.num_packets, loss_report.receive_time);
    return NetworkControlUpdate();;
}

NetworkControlUpdate GoogCcNetworkController::OnTransportPacketsFeedback(const TransportPacketsFeedback& report) {
    if (report.packet_feedbacks.empty()) {
        return NetworkControlUpdate();
    }
    TimeDelta max_feedback_rtt = TimeDelta::MinusInfinity();
    TimeDelta min_propagation_rtt = TimeDelta::PlusInfinity();
    size_t num_packets_received = 0;
  
    std::vector<PacketResult> received_packets = report.ReceivedPackets();
    for (const auto& packet : received_packets) {
        // Calculate propagation RTT: 
        // propagation_rtt = (report.recv_time - packet.send_time) - (last_packet.recv_time - packet.recv_time)
        //                    |              |
        // packet.send_time   +__            |
        //                    |  \________   |
        //                    |           \__+  packet.recv_time
        //                    |              |
        //                    |              | -> pending_time
        //                    |              |
        //                    |            __+  last_packet.recv_time
        //                    |   ________/  |
        // report.recv_time   +__/           |
        //                    |              |
        TimeDelta feedback_rtt = report.receive_time - packet.sent_packet.send_time;
        // NOTE: Fixed a bug to calcualte propagation RTT.
        // see: https://bugs.chromium.org/p/webrtc/issues/detail?id=13106&q=&can=1
        TimeDelta pending_time = report.last_acked_recv_time - packet.recv_time;
        TimeDelta propagation_rtt = feedback_rtt - pending_time;
        max_feedback_rtt = std::max(max_feedback_rtt, feedback_rtt);
        min_propagation_rtt = std::min(min_propagation_rtt, propagation_rtt);
        num_packets_received += 1;
    }

    // Update progation RTT.
    if (max_feedback_rtt.IsFinite()) {
        feedback_max_rtts_.push_back(max_feedback_rtt.ms());
        // Start to update the propagation RTT once reaching a certain amount.
        if (feedback_max_rtts_.size() > kMaxFeedbackRttWindow) {
            feedback_max_rtts_.pop_front();
            send_side_bwe_->OnPropagationRtt(min_propagation_rtt, report.receive_time);
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

        if (min_propagation_rtt.IsFinite()) {
            // Used to predict NACK round trip time in FEC controller.
            send_side_bwe_->OnRtt(min_propagation_rtt, report.receive_time);
        }

        // Loss information
        received_packets_since_last_loss_update_ += report.packet_feedbacks.size();
        lost_packets_since_last_loss_update_ += (report.packet_feedbacks.size() - num_packets_received);
        // Time to update loss info.
        if (TimeToUpdateLoss(report.receive_time)) {
            send_side_bwe_->OnPacketsLost(/*num_packets_lost=*/lost_packets_since_last_loss_update_, 
                                          /*num_packets*/received_packets_since_last_loss_update_, 
                                          report.receive_time);
            // Reset loss info after updating.
            received_packets_since_last_loss_update_ = 0;
            lost_packets_since_last_loss_update_ = 0;
        }
    }

    auto sorted_received_packets = report.SortedByReceiveTime();
    acknowledged_bitrate_estimator_->IncomingPacketFeedbacks(sorted_received_packets);
    auto acknowledged_bitrate = acknowledged_bitrate_estimator_->Estimate();
    send_side_bwe_->OnAcknowledgeBitrate(acknowledged_bitrate, report.receive_time);

    send_side_bwe_->IncomingPacketFeedbacks(report);
    // Ready to estimate the probe birate.
    for (const auto& feedback : sorted_received_packets) {
        if (feedback.sent_packet.pacing_info.probe_cluster) {
            probe_bitrate_estimator_->IncomingProbePacketFeedback(feedback);
        }
    }
    auto probe_bitrate = probe_bitrate_estimator_->Estimate();
    if (limit_probes_lower_than_throughput_estimate_ && probe_bitrate && acknowledged_bitrate) {
        // Limit the backoff to slightly below the acknowledged bitrate, because we want 
        // to drain the queues if we are actually overusing.
        auto backoff_bitrate = acknowledged_bitrate.value() * kProbeDropThroughputFraction;
        // The acknowledged bitrate shouldn't normally be higher than the delay based estimate,
        // but it could happen (e.g. due to packet bursts or encoder overshoot.). we use
        // std::min to ensure a probe bitrate below the current BWE never causes an increase.
        DataRate curr_bwe = std::min(delay_based_bwe_->last_estimate(), backoff_bitrate);
        // If the probe bitrate lower then the current BWE, using the current BWE instead.
        // Since the probe bitrate has a higher priority than the acknowledged bitrate (in
        // delay_based_bwe) in non-overusing state.
        probe_bitrate = std::max(*probe_bitrate, curr_bwe);
    }

    NetworkControlUpdate update;
    auto result = delay_based_bwe_->IncomingPacketFeedbacks(report,
                                                            acknowledged_bitrate,
                                                            probe_bitrate,
                                                            false);
    // The delay-based estimate has updated.
    if (result.updated) {
        if (result.probe) {
            send_side_bwe_->OnSendBitrate(result.target_bitrate, report.receive_time);
        }
        send_side_bwe_->OnDelayBasedBitrate(result.target_bitrate, report.receive_time);
        // Update the estimate in the ProbeController, in case we want to probe.
        MaybeTriggerOnNetworkChanged(&update, report.receive_time);
    }

    // TODO: Implemets Probe controller and ALR detector
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

bool GoogCcNetworkController::TimeToUpdateLoss(Timestamp at_time) {
    if (at_time.IsFinite() && at_time > time_to_next_loss_update_) {
        time_to_next_loss_update_ = at_time + kLossUpdateInterval;
        return true;
    } else {
        return false;
    }
}
    
} // namespace naivertc
