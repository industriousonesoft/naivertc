#include "rtc/base/packet.hpp"

namespace naivertc {

Packet::Packet(size_t capacity) 
    : BinaryBuffer(),
    dscp_(0) {
    BinaryBuffer::reserve(capacity);
}

Packet::Packet(const uint8_t* bytes, size_t size) 
    : BinaryBuffer(bytes, bytes + size),
    dscp_(0) {}

Packet::Packet(const BinaryBuffer& raw_packet) 
    : BinaryBuffer(raw_packet),
    dscp_(0) {}

Packet::Packet(BinaryBuffer&& raw_packet) 
    : BinaryBuffer(std::move(raw_packet)),
    dscp_(0) {}

Packet::~Packet() {}

size_t Packet::dscp() const { 
    return dscp_; 
};

void Packet::set_dscp(size_t dscp) { 
    dscp_ = dscp; 
}
    
} // namespace naivertc

