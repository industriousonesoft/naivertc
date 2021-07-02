#include "base/packet.hpp"

namespace naivertc {

Packet::Packet(const char* data, size_t size) : dscp_(0) {
    // 使用reinterpret_cast(re+interpret+cast：重新诠释转型)对data中的数据格式进行重新映射: char -> byte
    auto begin = reinterpret_cast<const std::byte*>(data);
    bytes_.assign(begin, begin + size);
}

Packet::~Packet() {}

const char* Packet::data() const {
    return reinterpret_cast<const char*>(bytes_.data());
}

size_t Packet::size() const {
    return static_cast<size_t>(bytes_.size());
}
    
} // end of naivertc

