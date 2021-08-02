#include "rtc/rtp_rtcp/rtcp_packets/compound_packet.hpp"

namespace naivertc {
namespace rtcp {

CompoundPacket::CompoundPacket() = default;

CompoundPacket::~CompoundPacket() = default;

size_t CompoundPacket::PacketSize() const {
    size_t PacketSize = 0;
    for (const auto& appened : appended_packets_) {
        PacketSize += appened->PacketSize();
    }
    return PacketSize;
}

bool CompoundPacket::PackInto(uint8_t* buffer,
                              size_t* index,
                              size_t max_size,
                              PacketReadyCallback callback) const {
    for (const auto& appended : appended_packets_) {
        if (!appended->PackInto(buffer, index, max_size, callback)) {
            return false;
        }
    }
    return true;
}
    
} // namespace rtcp
} // namespace naivert 
