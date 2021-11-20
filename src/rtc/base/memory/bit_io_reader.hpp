#ifndef _RTC_BASE_BIT_BUFFER_H_
#define _RTC_BASE_BIT_BUFFER_H_

#include "base/defines.hpp"
#include "rtc/base/memory/bit_io.hpp"

namespace naivertc {

// Byte order is assumend big-endian/network
class RTC_CPP_EXPORT BitReader {
public:
    BitReader(const uint8_t* bytes, size_t byte_count);

    void GetCurrentOffset(size_t* out_byte_offset, size_t* out_bit_offset);
    uint64_t RemainingBitCount() const;

    // Reads bit-sized values from the buffer.
    // Return false if there isn't enough data left for the specified bit cout.
    template <typename T>
    bool ReadBits(size_t bit_count, T& val);

    template <typename T>
    bool ReadByte(T& val);

    bool ReadExpGolomb(uint32_t& val);
    bool ReadSignedExpGolomb(int32_t& val);

    // Peeks bit-sized values from the buffer.
    // Return false if there isn't enough data left for the specified bit cout.
    template <typename T>
    bool PeekBits(size_t bit_count, T& val);

    // Moves current position `bit_count` bytes forward. 
    // Returns false if there aren't enough bytes left in the buffer.
    bool ConsumeBits(size_t bit_count);

    // Sets the current offset to the provided byte/bit offsets.
    // The bit offset is from the given byte, in the range [0,7]
    bool Seek(size_t byte_offset, size_t bit_offset);
  
private:
    const uint8_t* const bytes_;
    size_t byte_count_;
    size_t byte_offset_;
    size_t bit_offset_;

    DISALLOW_COPY_AND_ASSIGN(BitReader);
};

template <typename T>
bool BitReader::ReadByte(T& val) {
    return ReadBits(sizeof(T) * 8, val);
}

template <typename T>
bool BitReader::ReadBits(size_t bit_count, T& val) {
    return PeekBits(bit_count, val) && ConsumeBits(bit_count);
}

template <typename T>
bool BitReader::PeekBits(size_t bit_count, T& val) {
    static_assert(std::is_integral<T>::value, "Type must be an integer.");
    if (bit_count > RemainingBitCount() || bit_count > (sizeof(T) * 8)) {
        return false;
    }
    const uint8_t* curr_bytes = bytes_ + byte_offset_;
    size_t remaining_bits_in_curr_byte = 8 - bit_offset_;
    T bits = RightMostBits(*curr_bytes++, remaining_bits_in_curr_byte);
    if (bit_count < remaining_bits_in_curr_byte) {
        // `remaining_bits` will expand to uint8_t with zero highest bits, 
        // so we need to count `bit_offset_` as bit count.
        val = LeftMostBits(bits, bit_offset_ + bit_count);
        return true;
    }
    bit_count -= remaining_bits_in_curr_byte;
    while (bit_count >= 8) {
        bits = (bits << 8) | *curr_bytes++;
        bit_count -= 8;
    }
    if (bit_count > 0) {
        bits <<= bit_count;
        bits |= LeftMostBits(*curr_bytes, bit_count);
    }
    val = bits;
    return true;
}

} // namespace naivertc

#endif