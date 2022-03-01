#include "rtc/congestion_controller/goog_cc/goog_cc_network_controller.hpp"

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

std::unique_ptr<CongestionWindwoPushbackController>
MayCreateCongestionWindwoPushbackController(const RateControlSettings& rate_control_settings) {
    if (rate_control_settings.UseCongestionWindowPushback()) {
        CongestionWindwoPushbackController::Configuration config;
        config.add_pacing = true;
        config.initial_congestion_window = rate_control_settings.initial_congestion_window.value_or(0);
        config.min_pushback_bitrate = *rate_control_settings.min_pushback_bitrate;
        return std::make_unique<CongestionWindwoPushbackController>(config);
    } else {
        return nullptr;
    }
}
    
} // namespace

GoogCcNetworkController::GoogCcNetworkController(const Configuration& config) 
    : packet_feedback_only_(false),
      use_min_allocated_bitrate_as_lower_bound_(false),
      limit_probes_lower_than_throughput_estimate_(false),
      use_loss_based_as_stable_bitrate_(false),
      rate_control_settings_(config.rate_control_settings),
      send_side_bwe_(std::make_unique<SendSideBwe>(SendSideBwe::Configuration())),
      delay_based_bwe_(std::make_unique<DelayBasedBwe>(DelayBasedBwe::Configuration())),
      acknowledged_bitrate_estimator_(AcknowledgedBitrateEstimator::Create(ThroughputEstimator::Configuration())),
      probe_controller_(std::make_unique<ProbeController>(ProbeController::Configuration())),
      probe_bitrate_estimator_(std::make_unique<ProbeBitrateEstimator>()),
      alr_detector_(std::make_unique<AlrDetector>(AlrDetector::Configuration(), config.clock)),
      cwnd_controller_(MayCreateCongestionWindwoPushbackController(rate_control_settings_)),
      last_loss_based_target_bitrate_(config.constraints.starting_bitrate.value_or(DataRate::Zero())),
      last_stable_target_bitrate_(last_loss_based_target_bitrate_),
      last_pushback_target_bitrate_(last_stable_target_bitrate_),
      pacing_factor_(config.stream_based_config.pacing_factor.value_or(kDefaultPaceMultiplier)),
      max_padding_bitrate_(config.stream_based_config.allocated_bitrate_limits.max_padding_bitrate),
      min_total_allocated_bitrate_(config.stream_based_config.allocated_bitrate_limits.min_total_allocated_bitrate),
      max_total_allocated_bitrate_(config.stream_based_config.allocated_bitrate_limits.max_total_allocated_bitrate),
      initial_config_(std::move(config)) {
    assert(config.clock != nullptr);
    // Initial estimate with the min bitrate.
    delay_based_bwe_->SetMinBitrate(kDefaultMinBitrate);
}

GoogCcNetworkController::~GoogCcNetworkController() {}

NetworkControlUpdate GoogCcNetworkController::OnNetworkAvailability(const NetworkAvailability& msg) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnNetworkRouteChange(const NetworkRouteChange& msg) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnPeriodicUpdate(const PeriodicUpdate& msg) {
    NetworkControlUpdate update;
    // Check the ALR state periodiclly.
    auto alr_started_time = alr_detector_->alr_started_time();
    probe_controller_->set_alr_start_time(alr_started_time);

    // Config at the first time.
    if (initial_config_) {
        update.probe_cluster_configs = ResetConstraints(initial_config_->constraints);
        update.pacer_config = GetPacerConfig(msg.at_time);
        // Enable probe in ALR periodiclly.
        if (initial_config_->stream_based_config.request_alr_probing) {
            probe_controller_->set_enable_periodic_alr_probing(*initial_config_->stream_based_config.request_alr_probing);
        }
        // Set the max allocated bitrate.
        auto max_total_bitrate = initial_config_->stream_based_config.allocated_bitrate_limits.max_total_allocated_bitrate;
        if (!max_total_bitrate.IsZero()) {
            auto probes = probe_controller_->OnMaxTotalAllocatedBitrate(max_total_bitrate, msg.at_time);
            // Appends the probes config
            update.AppendProbes(probes);
            max_total_allocated_bitrate_ = max_total_bitrate;
        }
        initial_config_.reset();
    }

    if (cwnd_controller_ && msg.pacer_queue) {
        cwnd_controller_->OnPacingQueue(*msg.pacer_queue);
    }

    // Update estimate periodiclly.
    send_side_bwe_->UpdateEstimate(msg.at_time);

    auto probes = probe_controller_->OnPeriodicProcess(msg.at_time);
    if (!probes.empty()) {
        update.AppendProbes(probes);
    }

    // Update congestion window.
    if (rate_control_settings_.UseCongestionWindow() && 
        last_packet_received_time_.IsFinite() && 
        !feedback_max_rtts_.empty()) {
        UpdateCongestionWindow();
    }
    if (cwnd_controller_ && curr_congestion_window_) {
        cwnd_controller_->set_congestion_window(*curr_congestion_window_);
    } else {
        update.congestion_window = *curr_congestion_window_;
    }

    MaybeTriggerOnNetworkChanged(&update, msg.at_time);
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
    // The ALR state might be changed after updating sent bytes, so we
    // should update the ALR state of |acknowledged_bitrate_estimator_|.
    alr_detector_->OnBytesSent(sent_packet.size, sent_packet.send_time);
    acknowledged_bitrate_estimator_->set_in_alr(alr_detector_->InAlr());

    if (!first_packet_sent_) {
        first_packet_sent_ = true;
        // Initialize feedback time to send time to allow estimation of RTT until
        // first feedback is received.
        send_side_bwe_->OnPropagationRtt(TimeDelta::Zero(), sent_packet.send_time);
    }
    send_side_bwe_->OnSentPacket(sent_packet);

    if (cwnd_controller_) {
        cwnd_controller_->OnInflightBytes(sent_packet.bytes_in_flight);
        NetworkControlUpdate update;
        MaybeTriggerOnNetworkChanged(&update, sent_packet.send_time);
        return update;
    } else {
        return NetworkControlUpdate();
    }
}

NetworkControlUpdate GoogCcNetworkController::OnReceivedPacket(const ReceivedPacket& received_packet) {
    last_packet_received_time_ = received_packet.receive_time;
    return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnStreamsConfig(const StreamsConfig& msg) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnTargetBitrateConstraints(const TargetBitrateConstraints& constraints) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnTransportLostReport(const TransportLossReport& loss_report) {
    if (packet_feedback_only_) {
        return NetworkControlUpdate();
    }
    send_side_bwe_->OnPacketsLostReport(loss_report.num_packets_lost, 
                                        loss_report.num_packets, 
                                        loss_report.receive_time);
    return NetworkControlUpdate();;
}

NetworkControlUpdate GoogCcNetworkController::OnTransportPacketsFeedback(const TransportPacketsFeedback& report) {
    if (report.packet_feedbacks.empty()) {
        return NetworkControlUpdate();
    }
    if (cwnd_controller_) {
        cwnd_controller_->OnInflightBytes(report.bytes_in_flight);
    }

    TimeDelta max_feedback_rtt = TimeDelta::MinusInfinity();
    TimeDelta min_propagation_rtt = TimeDelta::PlusInfinity();
  
    std::vector<PacketResult> received_packets = report.ReceivedPackets();
    const size_t num_packets_received = received_packets.size();
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
    }

    // Update progation RTT.
    if (max_feedback_rtt.IsFinite()) {
        feedback_max_rtts_.push_back(max_feedback_rtt);
        // FIXME: Why do we need to do update with a window here?
        if (feedback_max_rtts_.size() > kMaxFeedbackRttWindow) {
            feedback_max_rtts_.pop_front();
            // RTT will be employed to estimate a corrected RTT.
            // TODO: Use time since last unacknowledged packet instead of report receive time.
            send_side_bwe_->OnPropagationRtt(min_propagation_rtt, report.receive_time);
        }
    }

    // We need to retrieve the RTT and loss info from the packet feedbacks.
    if (packet_feedback_only_) {
        if (!feedback_max_rtts_.empty()) {
            // SMA: Simple Moving Average.
            TimeDelta sum_rtt = std::accumulate(feedback_max_rtts_.begin(), feedback_max_rtts_.end(), TimeDelta::Zero());
            TimeDelta mean_rtt = sum_rtt / feedback_max_rtts_.size();
            if (delay_based_bwe_) {
                // RTT will be employed to calculte the increase when used bandwidth 
                // is near the link capacity (assuming the max bitrate).
                // FIXME: Using feedback_rtt instread of propagation_rtt may be resulting a 
                // more smaller and more reasonable value?
                delay_based_bwe_->OnRttUpdate(mean_rtt);
            }
        }

        if (min_propagation_rtt.IsFinite()) {
            // RTT will be employed to predict NACK round trip time in FEC controller.
            send_side_bwe_->OnRtt(min_propagation_rtt, report.receive_time);
        }

        // Loss information
        received_packets_since_last_loss_update_ += report.packet_feedbacks.size();
        lost_packets_since_last_loss_update_ += (report.packet_feedbacks.size() - num_packets_received);
        // Time to update loss info.
        if (TimeToUpdateLoss(report.receive_time)) {
            send_side_bwe_->OnPacketsLostReport(/*num_packets_lost=*/lost_packets_since_last_loss_update_, 
                                                /*num_packets*/received_packets_since_last_loss_update_, 
                                                report.receive_time);
            // Reset loss info after updating.
            received_packets_since_last_loss_update_ = 0;
            lost_packets_since_last_loss_update_ = 0;
        }
    }

    auto arl_ended_time = alr_detector_->alr_ended_time();
    // if |arl_ended_time| has value indicates that we was in ALR 
    // but has quit from it now.
    if (arl_ended_time) {
        acknowledged_bitrate_estimator_->set_alr_ended_time(*arl_ended_time);
    }

    // Try to estimate the throughput with sorted feedbacks.
    auto sorted_received_packets = report.SortedByReceiveTime();
    acknowledged_bitrate_estimator_->IncomingPacketFeedbacks(sorted_received_packets);
    auto acknowledged_bitrate = acknowledged_bitrate_estimator_->Estimate();
    // The acknowledged estimate will be used to do estimation based on loss.
    send_side_bwe_->OnAcknowledgedBitrate(acknowledged_bitrate, report.receive_time);

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
        // Use the probed bitrate as the send birate.
        if (result.probe) {
            send_side_bwe_->OnSendBitrate(result.target_bitrate, report.receive_time);
        }
        send_side_bwe_->OnDelayBasedBitrate(result.target_bitrate, report.receive_time);
        // Update the estimate in the ProbeController, in case we want to probe.
        MaybeTriggerOnNetworkChanged(&update, report.receive_time);
    }

    if (result.recovered_from_underuse) {
        // We might be in ALR region when recovered from underuse.
        probe_controller_->set_alr_start_time(alr_detector_->alr_started_time());
        // Request a new probe.
        update.AppendProbes(probe_controller_->RequestProbe(report.receive_time));
    } else if (result.backoff_in_alr) {
        // If we just backed off during ALR, request a new probe.
        update.AppendProbes(probe_controller_->RequestProbe(report.receive_time));
    }

    if (rate_control_settings_.UseCongestionWindow() && 
        max_feedback_rtt.IsFinite()) {
        UpdateCongestionWindow();
    }

    if (cwnd_controller_ && curr_congestion_window_) {
        cwnd_controller_->set_congestion_window(*curr_congestion_window_);
    } else {
        update.congestion_window = *curr_congestion_window_;
    }

    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnNetworkStateEstimate(const NetworkEstimate& estimate) {
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
    update.congestion_window = curr_congestion_window_;
    return update;
}

// Private methods
void GoogCcNetworkController::MaybeTriggerOnNetworkChanged(NetworkControlUpdate* update, Timestamp at_time) {
    uint8_t fraction_loss = send_side_bwe_->fraction_loss();
    TimeDelta rtt = send_side_bwe_->rtt();
    DataRate loss_based_target_bitrate = send_side_bwe_->target_bitate();
    DataRate pushback_target_rate = loss_based_target_bitrate;
 
    double cwnd_reduce_ratio = 0.0;
    if (cwnd_controller_) {
        pushback_target_rate = cwnd_controller_->AdjustTargetBitrate(loss_based_target_bitrate);
        pushback_target_rate = std::max(send_side_bwe_->min_bitate(), pushback_target_rate);
        if (rate_control_settings_.drop_frame_only) {
            cwnd_reduce_ratio = static_cast<double>((loss_based_target_bitrate - pushback_target_rate) / loss_based_target_bitrate);
        }
    }

    DataRate stable_target_bitrate = send_side_bwe_->EstimatedLinkCapacity();
    // Use loss based target bitate as stable birate
    if (use_loss_based_as_stable_bitrate_) {
        stable_target_bitrate = std::min(stable_target_bitrate, loss_based_target_bitrate);
    } else {
        stable_target_bitrate = std::min(stable_target_bitrate, pushback_target_rate);
    }
    
    // FIXME: Check if the esitmation has been updated?
    if ((loss_based_target_bitrate != last_loss_based_target_bitrate_) ||
        (fraction_loss != last_estimated_fraction_loss_) ||
        (rtt != last_estimated_rtt) ||
        (pushback_target_rate != last_pushback_target_bitrate_) ||
        (stable_target_bitrate != last_stable_target_bitrate_)) {

        last_loss_based_target_bitrate_ = loss_based_target_bitrate;
        last_pushback_target_bitrate_ = pushback_target_rate;
        last_estimated_fraction_loss_ = fraction_loss;
        last_estimated_rtt = rtt;
        last_stable_target_bitrate_ = stable_target_bitrate;

        // Update the bitrate used to increase the ALR budget.
        alr_detector_->SetTargetBitrate(loss_based_target_bitrate);

        TimeDelta delay_bwe_period = delay_based_bwe_->GetExpectedBwePeriod();

        TargetTransferRate target_bitrate_msg;
        target_bitrate_msg.at_time = at_time;
        if (rate_control_settings_.drop_frame_only) {
            target_bitrate_msg.target_bitrate = loss_based_target_bitrate;
            target_bitrate_msg.cwnd_reduce_ratio = cwnd_reduce_ratio;
        } else {
            target_bitrate_msg.target_bitrate = pushback_target_rate;
        }
        target_bitrate_msg.stable_target_bitrate = stable_target_bitrate;
        target_bitrate_msg.network_estimate.at_time = at_time;
        target_bitrate_msg.network_estimate.rtt = rtt;
        target_bitrate_msg.network_estimate.loss_rate_ratio = fraction_loss / 255.0f;
        target_bitrate_msg.network_estimate.bwe_period = delay_bwe_period;
        update->target_rate = target_bitrate_msg;

        auto probes = probe_controller_->OnEstimatedBitrate(loss_based_target_bitrate, at_time);
        update->AppendProbes(probes);
        
        update->pacer_config = GetPacerConfig(at_time);

        PLOG_VERBOSE << "last_loss_based_target_bitrate_bps=" << loss_based_target_bitrate.bps()
                     << ", pushback_target_bitrate_bps=" << pushback_target_rate.bps()
                     << ", estimated_fraction_loss=" << fraction_loss
                     << ", estimated_rtt_ms=" << rtt.ms()
                     << ", stable_target_bitrate_bps=" << stable_target_bitrate.bps()
                     << ", at time: " << at_time.ms();
    }
        
}

bool GoogCcNetworkController::TimeToUpdateLoss(Timestamp at_time) {
    if (at_time.IsFinite() && at_time > time_to_next_loss_update_) {
        time_to_next_loss_update_ = at_time + kLossUpdateInterval;
        return true;
    } else {
        return false;
    }
}

std::vector<ProbeClusterConfig> GoogCcNetworkController::ResetConstraints(const TargetBitrateConstraints& new_constraints) {

    min_target_bitrate_ = new_constraints.min_bitrate.value_or(DataRate::Zero());
    max_bitrate_ = new_constraints.max_bitrate.value_or(DataRate::PlusInfinity());
    starting_bitrate_ = new_constraints.starting_bitrate;
    ClampConstraints();

    // Uses the start bitrate as the send bitrate at the first time.
    send_side_bwe_->OnBitrates(starting_bitrate_, min_bitrate_, max_bitrate_, new_constraints.at_time);

    if (starting_bitrate_) {
        delay_based_bwe_->SetStartBitrate(*starting_bitrate_);
    }
    delay_based_bwe_->SetMinBitrate(min_bitrate_);

    // Probes
    return probe_controller_->OnBitrates(starting_bitrate_.value_or(DataRate::Zero()),
                                         min_bitrate_,
                                         max_bitrate_, 
                                         new_constraints.at_time);
}

void GoogCcNetworkController::ClampConstraints() {
    min_bitrate_ = std::max(min_bitrate_, kDefaultMinBitrate);
    if (use_min_allocated_bitrate_as_lower_bound_) {
        min_bitrate_ = std::max(min_bitrate_, min_total_allocated_bitrate_);
    }
    if (max_bitrate_ < min_bitrate_) {
        PLOG_WARNING << "The max bitrate is smaller than the min bitrate.";
        max_bitrate_ = min_bitrate_;
    }
    if (starting_bitrate_ && starting_bitrate_ < min_bitrate_) {
        PLOG_WARNING << "The start bitrate is smaller than the min bitrate.";
        starting_bitrate_ = min_bitrate_;
    }
}

void GoogCcNetworkController::UpdateCongestionWindow() {
    auto min_feedback_max_rtt = *std::min_element(feedback_max_rtts_.begin(), feedback_max_rtts_.end());

    const size_t kMinCongestionWindow = 3000;
    // congestion time window = feedback_rtt + queuing_delay.
    TimeDelta time_window = min_feedback_max_rtt + rate_control_settings_.queuing_delay.value_or(TimeDelta::Zero());

    size_t congestion_window = static_cast<size_t>(last_loss_based_target_bitrate_.bps() * time_window.ms() / 8000);
    if (curr_congestion_window_) {
        congestion_window = std::max(kMinCongestionWindow, /*smooth filter*/(*curr_congestion_window_ + congestion_window) / 2);
    } else {
        congestion_window = std::max(kMinCongestionWindow, congestion_window);
    }
    curr_congestion_window_ = congestion_window;
}

PacerConfig GoogCcNetworkController::GetPacerConfig(Timestamp at_time) const {
    // Pacing bitrate is based on target bitrate before congestion window pushback,
    // because we don't want to build queues in the pacer when pushback occurs.
    DataRate pacing_bitrate = std::max(min_total_allocated_bitrate_, last_loss_based_target_bitrate_) * pacing_factor_;
    DataRate padding_bitrate = std::min(max_padding_bitrate_, last_pushback_target_bitrate_);
    PacerConfig config;
    config.at_time = at_time;
    config.time_window = TimeDelta::Seconds(1);
    config.data_window = pacing_bitrate.bps() * config.time_window.ms() / 8000;
    config.pad_window = padding_bitrate.bps() * config.time_window.ms() / 8000;
    return config;
}
    
} // namespace naivertc
