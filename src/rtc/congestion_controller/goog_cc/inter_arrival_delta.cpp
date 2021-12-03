#include "rtc/congestion_controller/goog_cc/inter_arrival_delta.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr TimeDelta kBurstDeltaThreshold = TimeDelta::Millis(5); // 5ms
constexpr TimeDelta kMaxBurstDuration = TimeDelta::Millis(100); // 100ms
    
} // namespace


InterArrivalDelta::InterArrivalDelta(TimeDelta send_time_group_span) 
    : send_time_group_span_(send_time_group_span),
      num_consecutive_reordered_packets_(0) {}

InterArrivalDelta::~InterArrivalDelta() = default;

bool InterArrivalDelta::ComputeDeltas(Timestamp send_time, 
                                      Timestamp arrival_time, 
                                      Timestamp system_time,
                                      size_t packet_size, 
                                      TimeDelta* send_time_delta,
                                      TimeDelta* arrival_time_delta,
                                      int* packet_size_delta) {
    bool calculated_deltas = false;
    // We don't have enough data to update the filter, 
    // so we store it untill we have two frames of data to process.
    if (!curr_packet_group_.IsStarted()) {
        curr_packet_group_.first_packet_send_time = send_time;
        curr_packet_group_.last_packet_send_time = send_time;
        curr_packet_group_.first_packet_arrival_time = arrival_time;
    } else if (curr_packet_group_.first_packet_send_time > send_time) {
        // Reordered packet.
        return false;
    } else if (IsNewPacketGroup(arrival_time, send_time)) {
        // Detected a new packet group, and the previous packet group is ready.
        if (prev_packet_group_.IsCompleted()) {
            *send_time_delta = curr_packet_group_.last_packet_send_time - prev_packet_group_.last_packet_send_time;
            *arrival_time_delta = curr_packet_group_.last_packet_arrival_time - prev_packet_group_.last_packet_arrival_time;
            TimeDelta system_time_delta = curr_packet_group_.last_system_time - prev_packet_group_.last_system_time;
            if (*arrival_time_delta - system_time_delta >= kArrivalTimeOffsetThreshold) {
                PLOG_WARNING << "The arrival time clock offset has changed (diff = "
                             << arrival_time_delta->ms() - system_time_delta.ms()
                             << " ms), resetting.";
                Reset();
                return false;
            }
            if (*arrival_time_delta < TimeDelta::Zero()) {
                // The group of packets has been reordered since receiving its local
                // arrival timestamp.
                ++num_consecutive_reordered_packets_;
                if (num_consecutive_reordered_packets_ >= kReorderedResetThreshold) {
                    PLOG_WARNING << "Packets between send burst arrived out of order, resetting."
                                 << " arrival_time_delta = " << arrival_time_delta->ms()
                                 << ", send_time_delta = " << send_time_delta->ms();
                    Reset();
                }
                return false;
            } else {
                num_consecutive_reordered_packets_ = 0;
            }
            *packet_size_delta = static_cast<int>(curr_packet_group_.size) - static_cast<int>(prev_packet_group_.size);
            calculated_deltas = true;
        }
        prev_packet_group_ = curr_packet_group_;
        // Reset the current packet group
        curr_packet_group_.first_packet_send_time = send_time;
        curr_packet_group_.last_packet_send_time = send_time;
        curr_packet_group_.first_packet_arrival_time = arrival_time;
        curr_packet_group_.size = 0;
    } else {
        // The arrival order of a group may be out of order,
        // but the send order of a group was assumed to be in the order,
        // so we should keep the max send time as the last one.
        curr_packet_group_.last_packet_send_time = std::max(curr_packet_group_.last_packet_send_time, send_time);
    }
    // Accumulate the packet size.
    curr_packet_group_.size += packet_size;
    curr_packet_group_.last_packet_arrival_time = arrival_time;
    curr_packet_group_.last_system_time = system_time;

    return calculated_deltas;
}

// Private methods
bool InterArrivalDelta::IsNewPacketGroup(Timestamp arrival_time, Timestamp send_time) {
    if (!curr_packet_group_.IsStarted()) {
        return false;
    } else if (DetectedABurst(arrival_time, send_time)) {
        // Filter the burst packets, which are not taken account into the a new packet group.
        return false;
    } else {
        // FIXME: Using send time instead of arrival time to calculate the time span of a packet group,
        // since the send time is in the order.
        return send_time - curr_packet_group_.first_packet_send_time > send_time_group_span_;
    }
}

bool InterArrivalDelta::DetectedABurst(Timestamp arrival_time, Timestamp send_time) {
    assert(curr_packet_group_.IsCompleted());
    TimeDelta send_time_delta = send_time - curr_packet_group_.last_packet_send_time;
    if (send_time_delta.IsZero()) {
        return true;
    }
    TimeDelta arrival_time_delta = arrival_time - curr_packet_group_.last_packet_arrival_time;
    TimeDelta transport_delay = arrival_time_delta - send_time_delta;
    // The conditions to detect a burst:
    // 1. there have one or more packets dropped during transport (transport_delay_ms < 0);
    // 2. the interval between two arrival packet is too small (<= 5ms);
    // 3. the arrival span of the current packet group is too small (<= 100ms)
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
