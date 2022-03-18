#ifndef _RTC_CONGESTION_CONTROL_RECEIVE_SIDE_PACKET_ARRIVAL_TIME_MAP_H_
#define _RTC_CONGESTION_CONTROL_RECEIVE_SIDE_PACKET_ARRIVAL_TIME_MAP_H_

#include <deque>

namespace naivertc {

// PacketArrivalTimeMap is an optimized map of packet id to packet arrival
// time, limited in size to never exceed `kMaxNumberOfPackets`. It will grow as
// needed, and remove old packets, and will expand to allow earlier packets to
// be added (out-of-order).
class PacketArrivalTimeMap {
public:
    static constexpr size_t kMaxNumberOfPackets = (1 << 15); // 32768
public:

    int64_t begin_packet_id() const;
    int64_t end_packet_id() const;

    // Returns an element with the index is equal to `packtet_id`.
    int64_t at(int64_t packtet_id) const;

    // Return true on the packet with `packet_id` has already been received.
    bool HasReceived(int64_t packet_id) const;

    void AddPacket(int64_t packet_id,
                   int64_t arrival_time_ms);

    void EraseTo(int64_t packet_id);

    // Removes packets from the beginning of the map as long as they are received
    // before `packet_id` and with an age older than `arrival_time_ms`.
    void RemoveOldPackets(int64_t packet_id,
                          int64_t arrival_time_ms);

    int64_t Clamp(int64_t packet_id) const;

private:
    std::deque<int64_t> arrival_times_;

    // The packet id (unwrapped sequence number) for the 
    // first elements in `arrival_times`.
    int64_t begin_packet_id_ = 0;

    // Indicates if this map has had any packet added to it.
    // The first packet decides the initial sequence number.
    bool has_received_packet_ = false;
};
    
} // namespace naivertc


#endif