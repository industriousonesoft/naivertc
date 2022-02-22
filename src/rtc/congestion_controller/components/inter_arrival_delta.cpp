#include "rtc/congestion_controller/components/inter_arrival_delta.hpp"

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

std::optional<InterArrivalDelta::Result> InterArrivalDelta::ComputeDeltas(Timestamp send_time, 
                                                                          Timestamp arrival_time, 
                                                                          Timestamp system_time,
                                                                          size_t packet_size) {
    std::optional<InterArrivalDelta::Result> ret = std::nullopt;
    // We don't have enough data to update the filter, 
    // so we store it untill we have two frames of data to process.
    if (!curr_packet_group_.IsStarted()) {
        curr_packet_group_.first_packet_send_time = send_time;
        curr_packet_group_.last_packet_send_time = send_time;
        curr_packet_group_.first_packet_arrival_time = arrival_time;
    } else if (curr_packet_group_.first_packet_send_time > send_time) {
        // Reordered packet.
        return std::nullopt;
    } else if (IsNewPacketGroup(arrival_time, send_time)) {
        // Detected a new packet group, and the previous packet group is ready.
        InterArrivalDelta::Result deltas;
        if (prev_packet_group_.IsCompleted()) {
            // Inter-depature
            deltas.send_time_delta = curr_packet_group_.last_packet_send_time - prev_packet_group_.last_packet_send_time;
            // Inter-arrival
            deltas.arrival_time_delta = curr_packet_group_.last_packet_arrival_time - prev_packet_group_.last_packet_arrival_time;
            TimeDelta system_time_delta = curr_packet_group_.last_system_time - prev_packet_group_.last_system_time;
            if (deltas.arrival_time_delta - system_time_delta >= kArrivalTimeOffsetThreshold) {
                PLOG_WARNING << "The arrival time clock offset has changed (diff = "
                             << deltas.arrival_time_delta.ms() - system_time_delta.ms()
                             << " ms), resetting.";
                Reset();
                return std::nullopt;
            }
            if (deltas.arrival_time_delta < TimeDelta::Zero()) {
                // The group of packets has been reordered since receiving its local
                // arrival timestamp.
                ++num_consecutive_reordered_packets_;
                if (num_consecutive_reordered_packets_ >= kReorderedResetThreshold) {
                    PLOG_WARNING << "Packets between send burst arrived out of order, resetting."
                                 << " arrival_time_delta = " << deltas.arrival_time_delta.ms()
                                 << ", send_time_delta = " << deltas.send_time_delta.ms();
                    Reset();
                }
                return std::nullopt;
            } else {
                num_consecutive_reordered_packets_ = 0;
            }
            deltas.packet_size_delta = static_cast<int>(curr_packet_group_.size) - static_cast<int>(prev_packet_group_.size);
            ret.emplace(std::move(deltas));
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

    return ret;
}

// Private methods
bool InterArrivalDelta::IsNewPacketGroup(Timestamp arrival_time, Timestamp send_time) {
    if (!curr_packet_group_.IsStarted()) {
        return false;
    }
    // The pre-filtering aims at handling delay transients caused by channel outages.
    // During an outage, packets being queued in network buffers, for reasons unrelated 
    // to congestion, are delivered in a burst when the outage ends.
    // The pre-filtering merges together groups of packets that arrive in a burst. 
    // Packets are merged in the same group if one of these two conditions holds:
    if (BelongsToBurst(arrival_time, send_time)) {
        // 1. All packets that arrive in a burst will be merged in the current group.
        return false;
    } else {
        // 2. A sequence of packets which are sent within a burst_time interval constitue a group,
        // otherwise, the incoming packet is the first packet of new group. since the Pacer 
        // sends a group of packets to the network every burst_time interval.
        return send_time - curr_packet_group_.first_packet_send_time > send_time_group_span_;
    }
}

bool InterArrivalDelta::BelongsToBurst(Timestamp arrival_time, Timestamp send_time) {
    assert(curr_packet_group_.IsCompleted());
    TimeDelta send_time_delta = send_time - curr_packet_group_.last_packet_send_time;
    if (send_time_delta.IsZero()) {
        return true;
    }
    TimeDelta arrival_time_delta = arrival_time - curr_packet_group_.last_packet_arrival_time;
    // Inter-group delay variation
    TimeDelta propagation_delta = arrival_time_delta - send_time_delta;
    // A Packet belongs to a burst if all the three conditions holds:
    // 1. A packet which has an inter-group delay variation less then 0;
    // 2. A packet which has an inter-arrival time less then burst_time (5ms);
    // 3. the arrival span of the current packet group is too small (100ms)
    if (propagation_delta < TimeDelta::Zero() && 
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
