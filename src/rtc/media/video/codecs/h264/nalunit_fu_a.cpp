#include "rtc/media/video/codecs/h264/nalunit_fu_a.hpp"

#include <cmath>

namespace naivertc {
namespace H264 {

NalUnit_FU_A::NalUnit_FU_A(FragmentType type, bool forbidden_bit, uint8_t nri, 
                                   uint8_t unit_type, BinaryBuffer payload_data) 
    : NalUnit(payload_data.size() + 2) {
    set_forbidden_bit(forbidden_bit);
    set_nri(nri);
    NalUnit::set_unit_type(kNalUnitTypeFuA);
    set_fragment_type(type);
    set_unit_type(unit_type);
    std::copy(payload_data.begin(), payload_data.end(), begin() + 2);
}

NalUnit_FU_A::NalUnit_FU_A(FragmentType type, bool forbidden_bit, uint8_t nri, 
                                   uint8_t unit_type, const uint8_t* payload_buffer, 
                                   size_t payload_size) 
    : NalUnit_FU_A(type, forbidden_bit, nri, unit_type, BinaryBuffer(payload_buffer, payload_buffer + payload_size)) {}
// Getter
bool NalUnit_FU_A::is_start() const { 
    return at(1) >> 7; 
}

bool NalUnit_FU_A::is_end() const { 
    return (at(1) >> 6) & 0x01; 
}

bool NalUnit_FU_A::is_reserved_bit_set() const { 
    return (at(1) >> 5 & 0x01); 
}

uint8_t NalUnit_FU_A::unit_type() const { 
    return at(1) & 0x1F; 
}

NalUnit_FU_A::FragmentType NalUnit_FU_A::fragment_type() const {
    if (is_start()) {
        return FragmentType::START;
    } else if (is_end()) {
        return FragmentType::END;
    } else {
        return FragmentType::MIDDLE;
    }
}

BinaryBuffer NalUnit_FU_A::payload() const {
    assert(size() >= 2);
    return {begin() + 2, end()};
}

// Setter
void NalUnit_FU_A::set_start(bool is_set) { 
    at(1) = (at(1) & 0x7F) | (uint8_t(is_set ? 1 : 0) << 7); 
}

void NalUnit_FU_A::set_end(bool is_set) { 
    at(1) = (at(1) & 0xBF) | (uint8_t(is_set ? 1 : 0) << 6); 
}

void NalUnit_FU_A::set_reserved_bit(bool is_set) { 
    at(1) = (at(1) & 0xDF) | uint8_t(is_set ? 1 : 0) << 5; 
}

void NalUnit_FU_A::set_unit_type(uint8_t type) { 
    at(1) = (at(1) & 0xE0) | (type & 0x1F); 
}

void NalUnit_FU_A::set_fragment_type(FragmentType type) {
    if (type == FragmentType::START) {
        set_start(true);
        set_end(false);
    } else if (type == FragmentType::END) {
        set_start(false);
        set_end(true);
    } else {
        set_start(false);
        set_end(false);
    }
}
    
} // namespace H264
} // namespace naivertc