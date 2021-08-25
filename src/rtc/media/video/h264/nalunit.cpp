#include "rtc/media/video/h264/nalunit.hpp"

namespace naivertc {
namespace h264 {

NalUnit::NalUnit() : BinaryBuffer(1) {}

NalUnit::NalUnit(const NalUnit&) = default;

NalUnit::NalUnit(BinaryBuffer&& other) 
    : BinaryBuffer(std::move(other)) {}

NalUnit::NalUnit(size_t size, bool including_header) 
    : BinaryBuffer(size + (including_header ? 0 : 1)) {}

NalUnit::NalUnit(const uint8_t* buffer, size_t size) 
    : BinaryBuffer(buffer, buffer + size) {}

// Getter
bool NalUnit::forbidden_bit() const { 
    return at(0) >> 7; 
}

uint8_t NalUnit::nri() const { 
    return at(0) >> 5 & 0x03; 
}

uint8_t NalUnit::unit_type() const { 
    return at(0) & 0x1F; 
}

ArrayView<const uint8_t> NalUnit::payload() const {
    assert(size() >= 1);
    return ArrayView(data() + 1, size() - 1);
}

// Setter
void NalUnit::set_forbidden_bit(bool is_set) { 
    at(0) = (at(0) & 0x7F) | (uint8_t(is_set ? 1 : 0) << 7); 
}

void NalUnit::set_nri(uint8_t nri) { 
    at(0) = (at(0) & 0x9F) | ((nri & 0x03) << 5); 
}

void NalUnit::set_unit_type(uint8_t type) { 
    at(0) = (at(0) & 0xE0) | (type & 0x1F); 
}

void NalUnit::set_payload(const BinaryBuffer& payload) {
    assert(size() >= 1);
    erase(begin() + 1, end());
    insert(end(), payload.begin(), payload.end());
}

void NalUnit::set_payload(const uint8_t* buffer, size_t size) {
    set_payload(BinaryBuffer(buffer, buffer + size));
}
    
} // namespace h264
} // namespace naivertc
