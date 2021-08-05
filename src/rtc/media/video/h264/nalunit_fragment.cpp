#include "rtc/media/video/h264/nalunit_fragment.hpp"

#include <cmath>

namespace naivertc {
namespace h264 {

NalUnitFragmentA::NalUnitFragmentA(FragmentType type, bool forbidden_bit, uint8_t nri, 
                                   uint8_t unit_type, BinaryBuffer payload_data) 
    : NalUnit(payload_data.size() + 2) {
    set_forbidden_bit(forbidden_bit);
    set_nri(nri);
    NalUnit::set_unit_type(kNalUnitTypeFuA);
    set_fragment_type(type);
    set_unit_type(unit_type);
    std::copy(payload_data.begin(), payload_data.end(), begin() + 2);
}

NalUnitFragmentA::NalUnitFragmentA(FragmentType type, bool forbidden_bit, uint8_t nri, 
                                   uint8_t unit_type, const uint8_t* payload_buffer, 
                                   size_t payload_size) 
    : NalUnitFragmentA(type, forbidden_bit, nri, unit_type, BinaryBuffer(payload_buffer, payload_buffer + payload_size)) {}
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

uint8_t NalUnitFragmentA::unit_type() const { 
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

BinaryBuffer NalUnitFragmentA::payload() const {
    assert(size() >= 2);
    return {begin() + 2, end()};
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

std::vector<std::shared_ptr<NalUnitFragmentA>> 
NalUnitFragmentA::FragmentsFrom(std::shared_ptr<NalUnit> nalu, uint16_t max_fragment_size) {
    if (nalu->size() <= max_fragment_size) {
        // We need to change 'max_fragment_size' to have at least two fragments
        max_fragment_size = nalu->size() / 2;
    }
    auto fragments_cout = ceil(double(nalu->size()) / max_fragment_size);
    max_fragment_size = ceil(nalu->size() / fragments_cout);

    // 2 bytes for FU indicator and FU header
    max_fragment_size -= 2;
    auto forbidden_bit = nalu->forbidden_bit();
    uint8_t nri = nalu->nri() & 0x03;
    uint8_t unit_type = nalu->unit_type() & 0x1F;
    auto payload = nalu->payload();

    std::vector<std::shared_ptr<NalUnitFragmentA>> fragments;
    uint64_t offset = 0;
    while (offset < payload.size()) {
        FragmentType fragment_type;
        if (offset == 0) {
            fragment_type = FragmentType::START;
        }else if (offset + max_fragment_size < payload.size()) {
            fragment_type = FragmentType::MIDDLE;
        }else {
            if (offset + max_fragment_size > payload.size()) {
                max_fragment_size = payload.size() - offset;
            }
            fragment_type = FragmentType::END;
        }
        BinaryBuffer fragment_data = {payload.begin() + offset, payload.begin() + offset + max_fragment_size};
        fragments.push_back(std::make_shared<NalUnitFragmentA>(fragment_type, forbidden_bit, nri, unit_type, fragment_data));
        offset += max_fragment_size;
    }
    return fragments;
}
    
} // namespace h264
} // namespace naivertc