#ifndef _RTC_MEDIA_VIDEO_H264_NAL_UNIT_FU_A_H_
#define _RTC_MEDIA_VIDEO_H264_NAL_UNIT_FU_A_H_

#include "base/defines.hpp"
#include "rtc/media/video/h264/nalunit.hpp"

namespace naivertc {
namespace H264 {
// NAL unit fragment header, RFC 6184, Section 5.8
// +---------------+
// |0|1|2|3|4|5|6|7|
// +-+-+-+-+-+-+-+-+
// |S|E|R|  Type   |
// +---------------+

class RTC_CPP_EXPORT NalUnit_FU_A : public NalUnit {
public:
    enum class FragmentType { START, MIDDLE, END };
public:
    NalUnit_FU_A(FragmentType type, bool forbidden_bit, uint8_t nri, uint8_t unit_type, BinaryBuffer payload_data);
    NalUnit_FU_A(FragmentType type, bool forbidden_bit, uint8_t nri, uint8_t unit_type, const uint8_t* payload_buffer, size_t payload_size);

    bool is_start() const;
    bool is_end() const;
    bool is_reserved_bit_set() const;
    uint8_t unit_type() const;
    FragmentType fragment_type() const;

    BinaryBuffer payload() const;

    void set_start(bool is_set);
    void set_end(bool is_set);
    void set_reserved_bit(bool is_set);
    void set_unit_type(uint8_t type);
    void set_fragment_type(FragmentType type);

protected:
    static constexpr uint8_t kNalUnitTypeFuA = 28;
};
    
} // namespace H264
} // namespace naivertc

#endif