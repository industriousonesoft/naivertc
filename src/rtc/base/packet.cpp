#include "rtc/base/packet.hpp"

namespace naivertc {

Packet::Packet(size_t capacity) 
    : CopyOnWriteBuffer(size_t(0), capacity),
      dscp_(0) {}

Packet::Packet(const uint8_t* bytes, size_t size) 
    : CopyOnWriteBuffer(bytes, size),
      dscp_(0) {}

Packet::Packet(const CopyOnWriteBuffer& raw_packet) 
    : CopyOnWriteBuffer(raw_packet),
      dscp_(0) {}

Packet::Packet(CopyOnWriteBuffer&& raw_packet) 
    : CopyOnWriteBuffer(std::move(raw_packet)),
      dscp_(0) {}

Packet::Packet(const BinaryBuffer& raw_packet)
    : CopyOnWriteBuffer(raw_packet),
      dscp_(0) {}

Packet::Packet(BinaryBuffer&& raw_packet)
    : CopyOnWriteBuffer(std::move(raw_packet)),
      dscp_(0) {}

Packet::~Packet() {}

size_t Packet::dscp() const { 
    return dscp_; 
};

void Packet::set_dscp(size_t dscp) { 
    dscp_ = dscp; 
}
    
} // namespace naivertc

