#include "base/packet.hpp"

namespace naivertc {

Packet::Packet(const std::byte* bytes, size_t size) 
    : dscp_(0) {
    bytes_.assign(bytes, bytes + size);
}

Packet::Packet(std::vector<std::byte>&& bytes) 
    : bytes_(std::move(bytes)),
    dscp_(0) {
}

Packet::~Packet() {}

const char* Packet::data() const {
    return reinterpret_cast<const char*>(bytes_.data());
}

size_t Packet::size() const {
    return static_cast<size_t>(bytes_.size());
}
    
} // namespace naivertc

