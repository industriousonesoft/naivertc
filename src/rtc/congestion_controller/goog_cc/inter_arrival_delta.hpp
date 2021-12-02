#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_INTER_ARRIVAL_DELTA_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_INTER_ARRIVAL_DELTA_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/units/timestamp.hpp"

namespace naivertc {

class RTC_CPP_EXPORT InterArrivalDelta {
public:
    // After this many packet groups received out of order InterArrival will
    // reset, assuming that clocks have made a jump.
    static constexpr int kReorderedResetThreshold = 3;
    static constexpr TimeDelta kArrivalTimeOffsetThreshold = TimeDelta::Seconds(3);
public:
    explicit InterArrivalDelta(TimeDelta packet_group_time_span);
    InterArrivalDelta() = delete;
    InterArrivalDelta(const InterArrivalDelta&) = delete;
    InterArrivalDelta& operator=(const InterArrivalDelta&) = delete;
    ~InterArrivalDelta();

    void Reset();

private:
    bool IsNewPacketGroup(Timestamp arrival_time, Timestamp send_time);
    bool DoseBurstHappen(Timestamp arrival_time, Timestamp send_time);

private:
    struct PacketGroup {
        bool IsFirstPacket() const { return first_packet_send_time.IsInfinite(); }

        void Reset() {
            size = 0;
            first_packet_send_time = Timestamp::MinusInfinity();
            first_packet_arrival_time = Timestamp::MinusInfinity();
            packet_send_time = Timestamp::MinusInfinity();
            packet_arrival_time = Timestamp::MinusInfinity();
            last_system_time = Timestamp::MinusInfinity();
        }

        size_t size = 0;
        Timestamp first_packet_send_time = Timestamp::MinusInfinity();
        Timestamp first_packet_arrival_time = Timestamp::MinusInfinity();
        Timestamp packet_send_time = Timestamp::MinusInfinity();
        Timestamp packet_arrival_time = Timestamp::MinusInfinity();
        Timestamp last_system_time = Timestamp::MinusInfinity();
    };
private:
    const TimeDelta packet_group_time_span_;
    PacketGroup curr_packet_group_;
    PacketGroup prev_packet_group_;
    size_t num_consecutive_reordered_packets_;
};
    
} // namespace naivertc


#endif