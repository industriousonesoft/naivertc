#include "base/packet.hpp"

namespace naivertc {

Packet::Packet(const uint8_t* bytes, size_t size) 
    : dscp_(0) {
    if (bytes != nullptr && size > 0) {
        bytes_.assign(bytes, bytes + size);
    }
}

Packet::Packet(std::vector<uint8_t>&& bytes) 
    : bytes_(std::move(bytes)),
    dscp_(0) {
}

Packet::~Packet() {}

const uint8_t* Packet::data() const {
    return bytes_.data();
}

uint8_t* Packet::data() {
    return bytes_.data();
}

const std::vector<uint8_t> Packet::bytes() const {
    return bytes_;
}

size_t Packet::size() const {
    return static_cast<size_t>(bytes_.size());
}

void Packet::Resize(size_t new_size) {
    bytes_.resize(new_size);
}
    
} // namespace naivertc

