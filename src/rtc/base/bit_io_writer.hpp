#ifndef _RTC_BASE_BIT_WRITER_H_
#define _RTC_BASE_BIT_WRITER_H_

#include "base/defines.hpp"
#include "rtc/base/bit_io.hpp"

namespace naivertc {

class RTC_CPP_EXPORT BitWriter {
public:
    BitWriter(uint8_t* bytes, size_t byte_count);

    void GetCurrentOffset(size_t* out_byte_offset, size_t* out_bit_offset);
    uint64_t RemainingBitCount() const;

    bool WriteBits(uint64_t val, size_t bit_count);

    template <typename T>
    bool WriteByte(T& val);

    bool WriteExpGolomb(uint32_t val);
    bool WriteSignedExpGolomb(int32_t val);

    bool Seek(size_t byte_offset, size_t bit_offset);

private:
    bool ConsumeBits(size_t bit_count);
    uint8_t WritePartialByte(uint8_t source,
                             size_t source_bit_count,
                             uint8_t target,
                             size_t target_bit_offset);
private:
    uint8_t* const bytes_;
    size_t byte_count_;
    size_t byte_offset_;
    size_t bit_offset_;

    DISALLOW_COPY_AND_ASSIGN(BitWriter);
};

template <typename T>
bool BitWriter::WriteByte(T& val) {
    return WriteBits(val, sizeof(T) * 8);
}
    
} // namespace naivertc


#endif