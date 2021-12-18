#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_GOOG_CC_NETWORK_CONTROLLER_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_GOOG_CC_NETWORK_CONTROLLER_H_

#include "base/defines.hpp"
#include "rtc/congestion_controller/network_controller_interface.hpp"
#include "rtc/congestion_controller/goog_cc/send_side_bwe.hpp"
#include "rtc/congestion_controller/goog_cc/delay_based_bwe.hpp"
#include "rtc/congestion_controller/goog_cc/acknowledged_bitrate_estimator.hpp"
#include "rtc/congestion_controller/goog_cc/probe_bitrate_estimator.hpp"

namespace naivertc {

class RTC_CPP_EXPORT GoogCcNetworkController : public NetworkControllerInterface {
public:
    GoogCcNetworkController(Configuration config);
    ~GoogCcNetworkController() override;

    // NetworkControllerInterface
    NetworkControlUpdate OnNetworkAvailability(NetworkAvailability) override;
    NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange) override;
    NetworkControlUpdate OnProcessInterval(ProcessInterval) override;
    NetworkControlUpdate OnRemoteBitrateReport(RemoteBitrateReport) override;
    NetworkControlUpdate OnRoundTripTimeUpdate(RoundTripTimeUpdate) override;
    NetworkControlUpdate OnSentPacket(SentPacket) override;
    NetworkControlUpdate OnReceivedPacket(ReceivedPacket) override;
    NetworkControlUpdate OnStreamsConfig(StreamsConfig) override;
    NetworkControlUpdate OnTargetBitrateConstraints(TargetBitrateConstraints) override;
    NetworkControlUpdate OnTransportLossReport(TransportLossReport) override;
    NetworkControlUpdate OnTransportPacketsFeedback(TransportPacketsFeedback) override;
    NetworkControlUpdate OnNetworkStateEstimate(NetworkEstimate) override;

    NetworkControlUpdate GetNetworkState(Timestamp at_time) const;

private:
    void MaybeTriggerOnNetworkChanged(NetworkControlUpdate* update, Timestamp at_time);

private:
    // Fixed variables
    const bool packet_feedback_only_;
    const bool use_min_allocated_bitrate_as_lower_bound_;
    const bool ignore_probes_lower_than_network_estimate_;
    const bool limit_probes_lower_than_throughput_estimate_;
    const bool loss_based_stable_bitrate_;

    std::unique_ptr<SendSideBwe> send_side_bwe_;
    std::unique_ptr<DelayBasedBwe> delay_based_bwe_;
    std::unique_ptr<AcknowledgedBitrateEstimator> ack_bitrate_estimator_;
    std::unique_ptr<ProbeBitrateEstimator> probe_bitrate_estimator_;

    DataRate min_target_bitrate = DataRate::Zero();
    DataRate min_bitrate = DataRate::Zero();
    DataRate max_bitrate = DataRate::PlusInfinity();
    std::optional<DataRate> starting_bitrate_;

    bool first_packet_sent_ = false;

    Timestamp time_next_loss_update_ = Timestamp::MinusInfinity();
    int lost_packets_since_last_loss_update_ = 0;
    int expected_packets_since_last_loss_update_ = 0;

    std::deque<int64_t> feedback_max_rtts_;

    std::optional<Configuration> initial_config_;

    DataRate last_loss_based_target_bitrate_;
    DataRate last_stable_target_bitrate_;

    std::optional<uint8_t> last_estimated_fraction_loss_ = 0;
    TimeDelta last_estimated_rtt = TimeDelta::PlusInfinity();
    Timestamp last_packet_received_time_ = Timestamp::MinusInfinity();

    double pacing_factor_;
    DataRate max_padding_bitrate_;
    DataRate min_total_allocated_bitrate_;
    DataRate max_total_allocated_bitrate_;

    bool previously_in_alr = false;

    std::optional<size_t> curr_data_window_;
};
    
} // namespace naivertc


#endif