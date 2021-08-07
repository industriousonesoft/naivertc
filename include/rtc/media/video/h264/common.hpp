#ifndef _RTC_VIDEO_H264_COMMON_H_
#define _RTC_VIDEO_H264_COMMON_H_

#include "base/defines.hpp"

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace naivertc {
namespace H264 {

// The size of a full NALU start sequence {0 0 0 1},
// used for the first NALU of an access unit, and for SPS and PPS block.
static constexpr size_t kNaluLongStartSequenceSize = 4;

// THe size of a shortened NALU start sequence {0 0 1}, 
// that may be used if not the first NALU of an access unit or SPS or PPS blocks.
static constexpr size_t kNaluShortStartSequenceSize = 3;

struct RTC_CPP_EXPORT NaluIndex {
    // Start index of NALU, including start sequence.
    size_t start_offset;
    // Start index of NALU payload, typically type header.
    size_t payload_start_offset;
    // Length of NALU payload, in bytes, counting fron payload_start_offset
    size_t payload_size;
};

// Returns a vector of the NALU indices in the given buffer.
std::vector<NaluIndex> FindNaluIndices(const uint8_t* buffer, size_t size);

// Packetization modes are defined in RFC 6184 section 6
// Due to the structure containing this being initialized 
// with zeroes in some places, and mode 1 (non-interleaved) 
// being default, mode 1 needs to have the value zero.
// https://crbug.com/webrtc/6803
enum class PacketizationMode {
    // Mode 1: SRAP-A, FU-A is allowed
    NON_INTERLEAVED = 0,
    // Mode 0: Only single NALU allowed
    SINGLE_NAL_UNIT
};
    
} // namespace h264
} // namespace naivertc


#endif