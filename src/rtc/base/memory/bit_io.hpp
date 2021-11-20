#ifndef _RTC_BASE_MEMORY_BIT_IO_H_
#define _RTC_BASE_MEMORY_BIT_IO_H_

#include "base/defines.hpp"

namespace naivertc {

// Returns the right-most `bit_count` bits in `byte`.
uint8_t RightMostBits(uint8_t byte, size_t bit_count);

// Returns the left-most `bit_count` bits in `byte`.
uint8_t LeftMostBits(uint8_t byte, size_t bit_count);

// Returns the left-most byte of `val` in a uint8_t.
uint8_t LeftMostByte(uint64_t val);

// Counts the number of bits used in the binary representation of val.
size_t CountBits(uint64_t val);
    
} // namespace naivertc


#endif