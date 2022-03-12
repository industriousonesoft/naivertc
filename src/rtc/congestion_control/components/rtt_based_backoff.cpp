#include "rtc/congestion_control/components/rtt_based_backoff.hpp"

namespace naivertc {

RttBasedBackoff::RttBasedBackoff() 
    : last_rtt_(TimeDelta::Zero()),
      time_last_rtt_update_(Timestamp::PlusInfinity()),
      time_last_packet_sent_(Timestamp::MinusInfinity()) {}

RttBasedBackoff::~RttBasedBackoff() = default;

void RttBasedBackoff::OnSentPacket(Timestamp at_time) {
    time_last_packet_sent_ = at_time;
}

void RttBasedBackoff::OnPropagationRtt(TimeDelta rtt,
                                       Timestamp at_time) {
    last_rtt_ = rtt;
    time_last_rtt_update_ = at_time;
}

TimeDelta RttBasedBackoff::CorrectedRtt(Timestamp at_time) const {
    TimeDelta time_since_rtt_updated = at_time - time_last_rtt_update_;
    TimeDelta time_since_packet_sent = at_time - time_last_packet_sent_;
    TimeDelta timeout_correction = std::max(time_since_rtt_updated - time_since_packet_sent, TimeDelta::Zero());
    return timeout_correction + last_rtt_;
}
    
} // namespace naivertc
