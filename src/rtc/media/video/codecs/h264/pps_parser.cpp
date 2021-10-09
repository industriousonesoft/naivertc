#include "rtc/media/video/codecs/h264/pps_parser.hpp"
#include "rtc/media/video/codecs/h264/nalunit.hpp"

namespace naivertc {

bool PpsParser::ParsePpsIds(const uint8_t* data, size_t size,  uint32_t* pps_id, uint32_t* sps_id) {
    std::vector<uint8_t> rbsp_buffer = h264::NalUnit::RetrieveRbsp(data, size);
    BitReader bit_reader(rbsp_buffer.data(), rbsp_buffer.size());
    return ParsePpsIdsInternal(bit_reader, pps_id, sps_id);
}

std::optional<uint32_t> PpsParser::ParsePpsIdsFromSlice(const uint8_t* data, size_t size) {
    std::vector<uint8_t> rbsp_buffer = h264::NalUnit::RetrieveRbsp(data, size);
    BitReader slice_reader(rbsp_buffer.data(), rbsp_buffer.size());
    uint32_t golomb_tmp;
    // first_mb_in_slice: ue(v)
    if (!slice_reader.ReadExpGolomb(golomb_tmp))
        return std::nullopt;
    // slice_type: ue(v)
    if (!slice_reader.ReadExpGolomb(golomb_tmp))
        return std::nullopt;
    // pic_parameter_set_id: ue(v)
    uint32_t slice_pps_id;
    if (!slice_reader.ReadExpGolomb(slice_pps_id))
        return std::nullopt;
    return slice_pps_id;
}

// Private methods
bool PpsParser::ParsePpsIdsInternal(BitReader& bit_reader, uint32_t* pps_id, uint32_t* sps_id) {
    if (pps_id != nullptr && bit_reader.ReadExpGolomb(*pps_id)) {
        return true;
    }else {
        return false;
    }
    if (sps_id != nullptr && bit_reader.ReadExpGolomb(*sps_id)) {
        return true;
    }else {
        return false;
    }
}
    
} // namespace naivertc
