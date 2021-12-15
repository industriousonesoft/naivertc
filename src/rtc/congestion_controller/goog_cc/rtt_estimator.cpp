#include "rtc/congestion_controller/goog_cc/rtt_estimator.hpp"

namespace naivertc {

RttEstimator::RttEstimator(Configuration config) 
    : config_(std::move(config)),
      last_rtt_(TimeDelta::Zero()),
      time_last_rtt_update_(Timestamp::PlusInfinity()),
      time_last_packet_sent_(Timestamp::MinusInfinity()) {}

RttEstimator::~RttEstimator() = default;

void RttEstimator::Update(TimeDelta rtt,
                          Timestamp at_time) {
    last_rtt_ = rtt;
    time_last_rtt_update_ = at_time;
}

void RttEstimator::OnSentPacket(const SendPacket& sent_packet) {
    time_last_packet_sent_ = sent_packet.send_time;
}

TimeDelta RttEstimator::Estimate() const {
    if (time_last_rtt_update_ > time_last_packet_sent_) {
        // The rtt was updated in time after sending the last packet.
        return last_rtt_;
    } else {
        // The last packet was set ,but the rtt is not updated until now.
        // TODO: Find a better mechanism to esitmate the RTT in future?
        TimeDelta timeout_correction = time_last_packet_sent_ - time_last_rtt_update_;
        return last_rtt_ + timeout_correction;
    }
}
    
} // namespace naivertc
