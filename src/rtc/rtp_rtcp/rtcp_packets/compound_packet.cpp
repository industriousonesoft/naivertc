#include "rtc/rtp_rtcp/rtcp_packets/compound_packet.hpp"

namespace naivertc {
namespace rtcp {

CompoundPacket::CompoundPacket() = default;

CompoundPacket::~CompoundPacket() = default;

size_t CompoundPacket::BlockLength() const {
    size_t block_length = 0;
    for (const auto& appened : appended_packets_) {
        block_length += appened->BlockLength();
    }
    return block_length;
}

bool CompoundPacket::Create(uint8_t* packet,
        size_t* index,
        size_t max_length,
        PacketReadyCallback callback) const {
    for (const auto& appended : appended_packets_) {
        if (!appended->Create(packet, index, max_length, callback)) {
            return false;
        }
    }
    return true;
}
    
} // namespace rtcp
} // namespace naivert 
