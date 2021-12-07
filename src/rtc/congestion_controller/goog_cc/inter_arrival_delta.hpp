#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_INTER_ARRIVAL_DELTA_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_INTER_ARRIVAL_DELTA_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/timestamp.hpp"

namespace naivertc {

// Helper class to compute the inter-arrival time delta and the size delta
// between two send bursts.
class RTC_CPP_EXPORT InterArrivalDelta {
public:
    // After this many packet groups received out of order InterArrival will
    // reset, assuming that clocks have made a jump.
    static constexpr int kReorderedResetThreshold = 3;
    static constexpr TimeDelta kArrivalTimeOffsetThreshold = TimeDelta::Seconds(3);
public:
    // NOTE: As the Pacer sends a group of packets to the network every burst_time 
    // interval. RECOMMENDED value for burst_time is 5 ms. 
    explicit InterArrivalDelta(TimeDelta send_time_group_span = TimeDelta::Millis(5));
    InterArrivalDelta() = delete;
    InterArrivalDelta(const InterArrivalDelta&) = delete;
    InterArrivalDelta& operator=(const InterArrivalDelta&) = delete;
    ~InterArrivalDelta();

    bool ComputeDeltas(Timestamp send_time, 
                       Timestamp arrival_time, 
                       Timestamp system_time,
                       size_t packet_size, 
                       TimeDelta* send_time_delta,
                       TimeDelta* arrival_time_delta,
                       int* packet_size_delta);

    void Reset();

private:
    // Check if the incoming packet is the first packet of a new pakcet group.
    bool IsNewPacketGroup(Timestamp arrival_time, Timestamp send_time);
    // Detecte if a burst happened.
    bool BelongsToBurst(Timestamp arrival_time, Timestamp send_time);

private:
    // Assume all the packets of a group belongs to a frame.
    struct PacketGroup {

        bool IsStarted() const { return first_packet_send_time.IsFinite(); }
        bool IsCompleted() const { return last_packet_arrival_time.IsFinite(); }

        void Reset() {
            size = 0;
            first_packet_send_time = Timestamp::MinusInfinity();
            first_packet_arrival_time = Timestamp::MinusInfinity();
            last_packet_send_time = Timestamp::MinusInfinity();
            last_packet_arrival_time = Timestamp::MinusInfinity();
            last_system_time = Timestamp::MinusInfinity();
        }

        size_t size = 0;
        Timestamp first_packet_send_time = Timestamp::MinusInfinity();
        Timestamp first_packet_arrival_time = Timestamp::MinusInfinity();
        Timestamp last_packet_send_time = Timestamp::MinusInfinity();
        Timestamp last_packet_arrival_time = Timestamp::MinusInfinity();
        Timestamp last_system_time = Timestamp::MinusInfinity();
    };
private:
    const TimeDelta send_time_group_span_;
    PacketGroup curr_packet_group_;
    PacketGroup prev_packet_group_;
    size_t num_consecutive_reordered_packets_;
};
    
} // namespace naivertc


#endif