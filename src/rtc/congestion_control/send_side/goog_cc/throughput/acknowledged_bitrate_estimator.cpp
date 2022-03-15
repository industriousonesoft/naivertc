#include "rtc/congestion_control/send_side/goog_cc/throughput/acknowledged_bitrate_estimator.hpp"

#include <algorithm>

namespace naivertc {

std::unique_ptr<AcknowledgedBitrateEstimator> AcknowledgedBitrateEstimator::Create(ThroughputEstimator::Configuration config) {
    return std::make_unique<AcknowledgedBitrateEstimator>(std::make_unique<ThroughputEstimator>(std::move(config)));
}

AcknowledgedBitrateEstimator::AcknowledgedBitrateEstimator(std::unique_ptr<ThroughputEstimator> bitrate_estimator) 
    : throughput_estimator_(std::move(bitrate_estimator)),
      in_alr_(false),
      alr_ended_time_(std::nullopt) {}

AcknowledgedBitrateEstimator::~AcknowledgedBitrateEstimator() {}

void AcknowledgedBitrateEstimator::set_in_alr(bool in_alr) {
    in_alr_ = in_alr;
}
    
void AcknowledgedBitrateEstimator::set_alr_ended_time(Timestamp alr_ended_time) {
    alr_ended_time_.emplace(alr_ended_time);
}

void AcknowledgedBitrateEstimator::IncomingPacketFeedbacks(const std::vector<PacketResult>& packet_feedbacks) {
    assert(std::is_sorted(packet_feedbacks.begin(),
                          packet_feedbacks.end(),
                          PacketResult::ReceiveTimeOrder()));
    for (const auto& packet_feedback : packet_feedbacks) {
        // Checks if the subsequent packets are in ALR or not.
        if (alr_ended_time_ && packet_feedback.sent_packet.send_time > *alr_ended_time_) {
            // Allows the bitrate to change fast as getting out of ALR.
            throughput_estimator_->ExpectFastRateChange();
            alr_ended_time_.reset();
        }
        // Computes the size of packets that have been received by remote.
        size_t acknowledged_packet_size = packet_feedback.sent_packet.size;
        // Accounts the bytes untracked by transport feedback but acknowledged
        // by remote with high probability, like the audio packet.
        acknowledged_packet_size += packet_feedback.sent_packet.prior_unacked_bytes;
        // Try to estimate the ackowledged bitrate.
        throughput_estimator_->Update(packet_feedback.recv_time, acknowledged_packet_size, in_alr_);
    }
}

std::optional<DataRate> AcknowledgedBitrateEstimator::Estimate() const {
    return throughput_estimator_->Estimate();
}
    
std::optional<DataRate> AcknowledgedBitrateEstimator::PeekRate() const {
    return throughput_estimator_->PeekRate();
}
    
} // namespace naivertc
