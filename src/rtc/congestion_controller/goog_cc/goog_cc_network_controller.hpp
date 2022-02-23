#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_GOOG_CC_NETWORK_CONTROLLER_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_GOOG_CC_NETWORK_CONTROLLER_H_

#include "rtc/congestion_controller/network_controller_interface.hpp"
#include "rtc/congestion_controller/goog_cc/send_side_bwe.hpp"
#include "rtc/congestion_controller/goog_cc/delay_based_bwe.hpp"
#include "rtc/congestion_controller/goog_cc/acknowledged_bitrate_estimator.hpp"
#include "rtc/congestion_controller/goog_cc/probe_bitrate_estimator.hpp"
#include "rtc/congestion_controller/goog_cc/probe_controller.hpp"
#include "rtc/congestion_controller/goog_cc/alr_detector.hpp"

namespace naivertc {

class GoogCcNetworkController : public NetworkControllerInterface {
public:
    GoogCcNetworkController(Configuration config);
    ~GoogCcNetworkController() override;

    // NetworkControllerInterface
    NetworkControlUpdate OnNetworkAvailability(NetworkAvailability) override;
    NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange) override;
    NetworkControlUpdate OnProcessInterval(ProcessInterval) override;
    NetworkControlUpdate OnRemoteBitrateUpdated(DataRate bitrate, Timestamp receive_time) override;
    NetworkControlUpdate OnRttUpdated(TimeDelta rtt, Timestamp receive_time) override;
    NetworkControlUpdate OnSentPacket(const SentPacket&) override;
    NetworkControlUpdate OnReceivedPacket(ReceivedPacket) override;
    NetworkControlUpdate OnStreamsConfig(StreamsConfig) override;
    NetworkControlUpdate OnTargetBitrateConstraints(TargetBitrateConstraints) override;
    NetworkControlUpdate OnTransportLostReport(const TransportLossReport&) override;
    NetworkControlUpdate OnTransportPacketsFeedback(const TransportPacketsFeedback&) override;
    NetworkControlUpdate OnNetworkStateEstimate(NetworkEstimate) override;

    NetworkControlUpdate GetNetworkState(Timestamp at_time) const;

private:
    void MaybeTriggerOnNetworkChanged(NetworkControlUpdate* update, Timestamp at_time);

    bool TimeToUpdateLoss(Timestamp at_time);
    std::vector<ProbeClusterConfig> ResetConstraints(TargetBitrateConstraints new_constraints);
    void ClampConstraints();

private:
    // Fixed variables
    const bool packet_feedback_only_;
    const bool use_min_allocated_bitrate_as_lower_bound_;
    const bool ignore_probes_lower_than_network_estimate_;
    const bool limit_probes_lower_than_throughput_estimate_;
    const bool use_loss_based_as_stable_bitrate_;

    std::unique_ptr<SendSideBwe> send_side_bwe_;
    std::unique_ptr<DelayBasedBwe> delay_based_bwe_;
    std::unique_ptr<AcknowledgedBitrateEstimator> acknowledged_bitrate_estimator_;
    std::unique_ptr<ProbeController> probe_controller_;
    std::unique_ptr<ProbeBitrateEstimator> probe_bitrate_estimator_;
    std::unique_ptr<AlrDetector> alr_detector_;
    
    DataRate min_target_bitrate_ = DataRate::Zero();
    DataRate min_bitrate_ = DataRate::Zero();
    DataRate max_bitrate_ = DataRate::PlusInfinity();
    std::optional<DataRate> starting_bitrate_;

    bool first_packet_sent_ = false;

    Timestamp time_to_next_loss_update_ = Timestamp::MinusInfinity();
    int lost_packets_since_last_loss_update_ = 0;
    int received_packets_since_last_loss_update_ = 0;

    std::deque<int64_t> feedback_max_rtts_;

    std::optional<Configuration> initial_config_;

    DataRate send_side_estimate_;
    DataRate last_stable_target_bitrate_;
    DataRate last_pushback_target_bitrate_;

    std::optional<uint8_t> last_estimated_fraction_loss_ = 0;
    TimeDelta last_estimated_rtt = TimeDelta::PlusInfinity();
    Timestamp last_packet_received_time_ = Timestamp::MinusInfinity();

    double pacing_factor_;
    DataRate max_padding_bitrate_;
    DataRate min_total_allocated_bitrate_;
    DataRate max_total_allocated_bitrate_;

    std::optional<size_t> curr_data_window_;
};
    
} // namespace naivertc


#endif