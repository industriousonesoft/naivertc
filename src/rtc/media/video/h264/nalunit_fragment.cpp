#include "rtc/media/video/h264/nalunit_fragment.hpp"

namespace naivertc {
namespace h264 {

NalUnitFragmentA::NalUnitFragmentA(FragmentType type, bool forbidden_bit, uint8_t nri, 
                                   uint8_t unit_type, BinaryBuffer payload_data) 
    : NalUnit(payload_data.size() + 2) {
    set_forbidden_zero_bit(forbidden_bit);
    set_nri(nri);
    NalUnit::set_unit_type(kNalUnitTypeFuA);
    set_fragment_type(type);
    set_unit_type(unit_type);
    std::copy(payload_data.begin(), payload_data.end(), begin() + 2);
}
// Getter
bool NalUnitFragmentA::is_start() const { 
    return at(1) >> 7; 
}

bool NalUnitFragmentA::is_end() const { 
    return (at(1) >> 6) & 0x01; 
}

bool NalUnitFragmentA::is_reserved_bit_set() const { 
    return (at(1) >> 5 & 0x01); 
}

uint8_t NalUnitFragmentA::uint_type() const { 
    return at(1) & 0x1F; 
}

NalUnitFragmentA::FragmentType NalUnitFragmentA::fragment_type() const {
    if (is_start()) {
        return FragmentType::START;
    }else if (is_end()) {
        return FragmentType::END;
    }else {
        return FragmentType::MIDDLE;
    }
}

// Setter
void NalUnitFragmentA::set_start(bool is_set) { 
    at(1) = (at(1) & 0x7F) | (uint8_t(is_set ? 1 : 0) << 7); 
}

void NalUnitFragmentA::set_end(bool is_set) { 
    at(1) = (at(1) & 0xBF) | (uint8_t(is_set ? 1 : 0) << 6); 
}

void NalUnitFragmentA::set_reserved_bit(bool is_set) { 
    at(1) = (at(1) & 0xDF) | uint8_t(is_set ? 1 : 0) << 5; 
}

void NalUnitFragmentA::set_unit_type(uint8_t type) { 
    at(1) = (at(1) & 0xE0) | (type & 0x1F); 
}

void NalUnitFragmentA::set_fragment_type(FragmentType type) {
    if (type == FragmentType::START) {
        set_start(true);
        set_end(false);
    }else if (type == FragmentType::END) {
        set_start(false);
        set_end(true);
    }else {
        set_start(false);
        set_end(false);
    }
}
    
} // namespace h264
} // namespace naivertc