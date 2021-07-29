#include "rtc/base/packet.hpp"

namespace naivertc {

Packet::Packet() 
    : BinaryBuffer(),
    dscp_(0) {}

Packet::Packet(const uint8_t* bytes, size_t size) 
    : BinaryBuffer(bytes, bytes + size),
    dscp_(0) {}

Packet::Packet(const Packet& other) 
    : BinaryBuffer(other),
    dscp_(other.dscp()) {}

Packet::Packet(const BinaryBuffer& buffer) 
    : BinaryBuffer(buffer),
    dscp_(0) {}

Packet::~Packet() {}

size_t Packet::dscp() const { 
    return dscp_; 
};

void Packet::set_dscp(size_t dscp) { 
    dscp_ = dscp; 
}
    
} // namespace naivertc

