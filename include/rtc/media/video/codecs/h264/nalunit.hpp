#ifndef _RTC_MEDIA_VIDEO_CODECS_H264_NAL_UNIT_H_
#define _RTC_MEDIA_VIDEO_CODECS_H264_NAL_UNIT_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"
#include "rtc/media/video/codecs/h264/common.hpp"

#include <vector>

namespace naivertc {
namespace h264 {
// NAL unit header, RFC 6184, Section 5.3
// +---------------+
// |0|1|2|3|4|5|6|7|
// +-+-+-+-+-+-+-+-+
// |F|NRI|  Type   |
// +---------------+
// F:
// 1 bit, 
// forbidden zero bit. The H264 specification requires that the F bit be equal to 0. 

// NRI: 
// 2 bits, 
// nal_ref_idc, indicates the relative transport priority.
// The highest transport priority is 11, followed by 10, and then by 01, finally, 00 is the lowest.

// Unit Type:
// 5 bits
// NAL Unit  Packet    Packet Type Name               Section
// Type      Type
// -------------------------------------------------------------
// 0        reserved                                     -
// 1-23     NAL unit  Single NAL unit packet             5.6
// 24       STAP-A    Single-time aggregation packet     5.7.1
// 25       STAP-B    Single-time aggregation packet     5.7.1
// 26       MTAP16    Multi-time aggregation packet      5.7.2
// 27       MTAP24    Multi-time aggregation packet      5.7.2
// 28       FU-A      Fragmentation unit                 5.8
// 29       FU-B      Fragmentation unit                 5.8
// 30-31    reserved  

class RTC_CPP_EXPORT NalUnit : public BinaryBuffer {
public:
    NalUnit();
    NalUnit(const NalUnit&);
    NalUnit(BinaryBuffer&&);
    NalUnit(const uint8_t* buffer, size_t size);
    NalUnit(size_t size, bool including_header = true);

    bool forbidden_bit() const;
    uint8_t nri() const;
    uint8_t unit_type() const;
    ArrayView<const uint8_t> payload() const;

    void set_forbidden_bit(bool is_set);
    void set_nri(uint8_t nri);
    void set_unit_type(uint8_t type);
    void set_payload(const BinaryBuffer& payload);
    void set_payload(const uint8_t* buffer, size_t size);
public:
    // Returns a vector of the NALU indices in the given buffer.
    static std::vector<NaluIndex> FindNaluIndices(const uint8_t* buffer, size_t size);

    // SODB: String of Data Bits, the raw encoded data and unprocessed.
    // RBSP: Raw Byte Sequence Payload, Added trailing bits(one RBSP Stop bit + zere or more 0 bits) based on SODB for memory alignment.
    // EBSP: Encapsulated Byte Sequence Paylaod, Added emulation byte (0x03) based on RBSP.
    
    // RBSP = SODB + RBSP Stop bit + 0 bits.
    // EBSP = RBSP Part_1 + 0x03 + RBSP Part_2 + 0x03 ... + RBSP + Part_n.
    // NALU = NALU Header + EBSP.
    // H264 Byte stream = start code + NALU + ... + start code + NALU.

    // Retrieve RBSP from EBSP by removing 0x03 emulation byte
    // See section 7.4.1 of the H264 spec.
    static std::vector<uint8_t> RetrieveRbspFromEbsp(const uint8_t* ebsp_buffer, size_t ebsp_size);
    static void WriteRbsp(const uint8_t* rbsp_buffer, size_t rbsp_size, std::vector<uint8_t>& ebsp_buffer);
};
    
} // namespace h264
} // namespace naivertc


#endif