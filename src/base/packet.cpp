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

char* Packet::data() {
    return reinterpret_cast<char*>(bytes_.data());
}

const std::vector<std::byte> Packet::bytes() const {
    return bytes_;
}

size_t Packet::size() const {
    return static_cast<size_t>(bytes_.size());
}

void Packet::Resize(size_t new_size) {
    bytes_.resize(new_size);
}
    
} // namespace naivertc

