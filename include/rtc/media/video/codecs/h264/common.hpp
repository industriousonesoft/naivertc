#ifndef _RTC_MEDIA_VIDEO_CODECS_H264_COMMON_H_
#define _RTC_MEDIA_VIDEO_CODECS_H264_COMMON_H_

#include "base/defines.hpp"

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace naivertc {
namespace h264 {

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

enum class PacketizationType {
    // This packet contains a single NAL unit.
    SIGNLE = 0,
    // This packet contains STAP-A (single time
    // aggregation) packets. If this packet has an
    // associated NAL unit type, it'll be for the
    // first such aggregated packet.
    STAP_A,
    // This packet contains a FU-A (fragmentation
    // unit) packet, meaning it is a part of a frame
    // that was too large to fit into a single packet.
    FU_A  
};

// Packetization modes are defined in RFC 6184 section 6
// Due to the structure containing this being initialized 
// with zeroes in some places, and mode 1 (non-interleaved) 
// being default, mode 1 needs to have the value zero.
// https://crbug.com/webrtc/6803
// https://datatracker.ietf.org/doc/html/rfc6184#section-6.0
enum class PacketizationMode {
    // Mode 1: STAP-A, FU-A is allowed
    NON_INTERLEAVED = 0,
    // Mode 0: Only single NALU allowed
    SINGLE_NAL_UNIT
};

enum NaluType : uint8_t {
    SLICE = 1,
    IDR = 5,
    SEI = 6,
    SPS = 7,
    PPS = 8,
    AUD = 9,
    END_OF_SEQUENCE = 10,
    END_OF_STREAM = 11,
    FILLER = 12,
    PREFIX = 14,
    STAP_A = 24,
    FU_A = 28
};

struct NaluInfo {
    uint8_t type;
    int sps_id;
    int pps_id;
    size_t offset;
    size_t size;
};

constexpr size_t kMaxNaluNumPerPacket = 10;

struct PacketizationInfo {
    // The packetization mode of this transport. Packetization mode
    // determines which packetization types are allowed when packetizing.
    PacketizationMode packetization_mode;
    // The packetization type of this buffer - single, aggregated or fragmented.
    PacketizationType packetization_type;
    
    // The NAL unit type of the original data for fragmented packet, or
    // the first NAL unit type in the packet for an aggregated packet.
    uint8_t packet_nalu_type;
    NaluInfo nalus[kMaxNaluNumPerPacket];
    size_t available_nalu_num = 0;

    bool has_sps = false;
    bool has_pps = false;
    bool has_idr = false;
};
    
} // namespace h264
} // namespace naivertc


#endif