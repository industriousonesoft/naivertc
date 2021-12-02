#include "rtc/congestion_controller/goog_cc/inter_arrival_delta.hpp"

namespace naivertc {
namespace {

constexpr TimeDelta kBurstDeltaThreshold = TimeDelta::Millis(5); // 5ms
constexpr TimeDelta kMaxBurstDuration = TimeDelta::Millis(100); // 100ms
    
} // namespace


InterArrivalDelta::InterArrivalDelta(TimeDelta packet_group_time_span) 
    : packet_group_time_span_(packet_group_time_span),
      num_consecutive_reordered_packets_(0) {}

InterArrivalDelta::~InterArrivalDelta() = default;

// Private methods
bool InterArrivalDelta::IsNewPacketGroup(Timestamp arrival_time, Timestamp send_time) {
    if (curr_packet_group_.IsFirstPacket()) {
        return false;
    } else if (DoseBurstHappen(arrival_time, send_time)) {
        return false;
    } else {
        return send_time - curr_packet_group_.first_packet_send_time > packet_group_time_span_;
    }
}

bool InterArrivalDelta::DoseBurstHappen(Timestamp arrival_time, Timestamp send_time) {
    assert(curr_packet_group_.packet_arrival_time.IsFinite());
    TimeDelta send_time_delta = send_time - curr_packet_group_.packet_send_time;
    if (send_time_delta.IsZero()) {
        return true;
    }
    TimeDelta arrival_time_delta = arrival_time - curr_packet_group_.packet_arrival_time;
    TimeDelta transport_delay = arrival_time_delta - send_time_delta;
    if (transport_delay < TimeDelta::Zero() && 
        arrival_time_delta <= kBurstDeltaThreshold &&
        arrival_time - curr_packet_group_.first_packet_arrival_time < kMaxBurstDuration) {
        return true;
    }
    return false;
}

void InterArrivalDelta::Reset() {
    num_consecutive_reordered_packets_ = 0;
    curr_packet_group_.Reset();
    prev_packet_group_.Reset();
}
    
} // namespace naivertc
