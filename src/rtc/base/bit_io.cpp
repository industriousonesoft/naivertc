#include "rtc/base/bit_io.hpp"

namespace naivertc {

uint8_t RightMostBits(uint8_t byte, size_t bit_count) {
    assert(bit_count <= 8);
    return byte & ((1 << bit_count) - 1);
}

uint8_t LeftMostBits(uint8_t byte, size_t bit_count) {
    assert(bit_count <= 8);
    uint8_t shift = 8 - static_cast<uint8_t>(bit_count);
    uint8_t mask = 0xFF << shift;
    return (byte & mask) >> shift;
}

uint8_t LeftMostByte(uint64_t val) {
   return static_cast<uint8_t>(val >> 56);
}

size_t CountBits(uint64_t val) {
  size_t bit_count = 0;
  while (val != 0) {
    bit_count++;
    val >>= 1;
  }
  return bit_count;
}
    
} // namespace naivertc
