#include "rtc/congestion_control/receive_side/packet_arrival_time_map.hpp"

#include "plog/Log.h"

#include <algorithm>

namespace naivertc {
namespace {

constexpr int64_t kPacketIdPlaceholder = 0;
    
} // namespace

int64_t PacketArrivalTimeMap::begin_packet_id() const {
    return begin_packet_id_;
}

int64_t PacketArrivalTimeMap::end_packet_id() const {
    return begin_packet_id_ + arrival_times_.size();
}

int64_t PacketArrivalTimeMap::at(int64_t packtet_id) const {
    auto offset = packtet_id - begin_packet_id_;
    if(offset < 0 || offset >= static_cast<int64_t>(arrival_times_.size())) {
        throw std::out_of_range("The packet id[" + std::to_string(packtet_id) + 
                                "] is not in rang[" + std::to_string(begin_packet_id()) + 
                                ", " + std::to_string(end_packet_id()) + ").");
    }
    return arrival_times_[offset];
}

void PacketArrivalTimeMap::AddPacket(int64_t packet_id,
                                     int64_t arrival_time_ms) {
    if (!has_received_packet_) {
        has_received_packet_ = true;
        begin_packet_id_ = packet_id;
        arrival_times_.push_back(arrival_time_ms);
        return;
    }

    int64_t offset = packet_id - begin_packet_id_;
    if (offset >= 0 && offset < static_cast<int64_t>(arrival_times_.size())) {
        // The packet is within the queue, no need to expand it.
        arrival_times_[offset] = arrival_time_ms;
        return;
    }

    // Out-of-order.
    if (offset < 0) {
        // The packet goes before the curren queue, so expand to add it,
        // but only if it fits within `kMaxNumberOfPackets`.
        size_t missing_packets = static_cast<size_t>(-offset);
        if (missing_packets + arrival_times_.size() > kMaxNumberOfPackets) {
            PLOG_WARNING << "The incoming packet[id=" << packet_id
                         << "] is out-of-order, and adding it would remove newly receive packet, droping it.";
            return;
        }
        arrival_times_.insert(arrival_times_.begin(), missing_packets, kPacketIdPlaceholder);
        arrival_times_[0] = arrival_time_ms;
        begin_packet_id_ = packet_id;
        return;
    }

    // The incoming packet goes after the queue.

    if (static_cast<size_t>(offset) >= kMaxNumberOfPackets) {
        // The queue grows too large, so the old packets have to be removed.
        size_t packets_to_remove = static_cast<size_t>(offset) - kMaxNumberOfPackets + 1;

        if (packets_to_remove >= arrival_times_.size()) {
            arrival_times_.clear();
            begin_packet_id_ = packet_id;
            offset = 0;
        } else {
            // Trim the queue by removing the leading non-received packets,
            // to ensure that the buffer only spans received packets.
            while (packets_to_remove < arrival_times_.size() && 
                   arrival_times_[packets_to_remove] == kPacketIdPlaceholder) {
                ++packets_to_remove;
            }

            arrival_times_.erase(arrival_times_.begin(), arrival_times_.begin() + packets_to_remove);
            begin_packet_id_ += packets_to_remove;
            // new_offset <= (kMaxNumberOfPackets - 1)
            offset -= packets_to_remove;
            assert(offset >= arrival_times_.size());
        }
    }

    // Packets can be received out-of-order. If this isn't the next continuous
    // packet, add enough placeholders to fill the gap.
    size_t missing_gap_packets = offset - arrival_times_.size();
    if (missing_gap_packets > 0) {
        arrival_times_.insert(arrival_times_.end(), missing_gap_packets, kPacketIdPlaceholder);
    }
    assert(arrival_times_.size() == offset);
    arrival_times_.push_back(arrival_time_ms);
    assert(arrival_times_.size() <= kMaxNumberOfPackets);
}

bool PacketArrivalTimeMap::HasReceived(int64_t packet_id) const {
    int64_t offset = packet_id - begin_packet_id_;
    if (offset >= 0 && offset < static_cast<int64_t>(arrival_times_.size()) &&
        arrival_times_[offset] != kPacketIdPlaceholder) {
        return true;
    }
    return false;
}

void PacketArrivalTimeMap::EraseTo(int64_t packet_id) {
    if (packet_id > begin_packet_id_) {
        size_t packets_to_remove = std::min(static_cast<size_t>(packet_id - begin_packet_id_), arrival_times_.size());

        arrival_times_.erase(arrival_times_.begin(), arrival_times_.begin() + packets_to_remove);
        begin_packet_id_ += packets_to_remove;
    }
}

void PacketArrivalTimeMap::RemoveOldPackets(int64_t packet_id,
                                            int64_t arrival_time_ms) {
    while (!arrival_times_.empty() && 
            begin_packet_id_ < packet_id &&
            arrival_times_.front() <= arrival_time_ms) {
        arrival_times_.pop_front();
        ++begin_packet_id_;
    }
}

int64_t PacketArrivalTimeMap::Clamp(int64_t packet_id) const {
    return std::min(end_packet_id(), std::max(packet_id, begin_packet_id()));
}
    
} // namespace naivertc
