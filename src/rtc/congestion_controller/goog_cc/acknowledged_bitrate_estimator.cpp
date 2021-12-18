#include "rtc/congestion_controller/goog_cc/acknowledged_bitrate_estimator.hpp"

#include <algorithm>

namespace naivertc {

std::unique_ptr<AcknowledgedBitrateEstimator> AcknowledgedBitrateEstimator::Create(BitrateEstimator::Configuration config) {
    return std::make_unique<AcknowledgedBitrateEstimator>(std::make_unique<BitrateEstimator>(std::move(config)));
}

AcknowledgedBitrateEstimator::AcknowledgedBitrateEstimator(std::unique_ptr<BitrateEstimator> bitrate_estimator) 
    : bitrate_estimator_(std::move(bitrate_estimator)),
      in_alr_(false),
      alr_ended_time_(std::nullopt) {}

AcknowledgedBitrateEstimator::~AcknowledgedBitrateEstimator() {
    bitrate_estimator_.reset();
}

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
        if (alr_ended_time_ && packet_feedback.sent_packet.send_time > *alr_ended_time_) {
            bitrate_estimator_->ExpectFastRateChange();
            alr_ended_time_.reset();
        }
        size_t acknowledged_packet_size = packet_feedback.sent_packet.size;
        acknowledged_packet_size += packet_feedback.sent_packet.prior_unacked_bytes;
        bitrate_estimator_->Update(packet_feedback.recv_time, acknowledged_packet_size, in_alr_);
    }
}

std::optional<DataRate> AcknowledgedBitrateEstimator::Estimate() const {
    return bitrate_estimator_->Estimate();
}
    
std::optional<DataRate> AcknowledgedBitrateEstimator::PeekRate() const {
    return bitrate_estimator_->PeekRate();
}
    
} // namespace naivertc
