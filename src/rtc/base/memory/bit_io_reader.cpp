#include "rtc/base/memory/bit_io_reader.hpp"

namespace naivertc {
namespace {

} // namespace


BitReader::BitReader(const uint8_t* bytes, size_t byte_count) 
    : bytes_(bytes),
      byte_count_(byte_count),
      byte_offset_(0),
      bit_offset_(0) {}

void BitReader::GetCurrentOffset(size_t* out_byte_offset,
                                 size_t* out_bit_offset) {
    assert(out_byte_offset != nullptr);
    assert(out_bit_offset != nullptr);
    *out_byte_offset = byte_offset_;
    *out_bit_offset = bit_offset_;
}

uint64_t BitReader::RemainingBitCount() const {
    return (static_cast<uint64_t>(byte_count_) - byte_offset_) * 8 - bit_offset_;
}

// See https://en.wikipedia.org/wiki/Exponential-Golomb_coding
bool BitReader::ReadExpGolomb(uint32_t& value) {
    size_t original_byte_offset = byte_offset_;
    size_t original_bit_offset = bit_offset_;

    size_t zero_bit_count = 0;
    uint32_t peeked_bit;
    // Count the number of leading 0 bits
    while(PeekBits(1, peeked_bit) && peeked_bit == 0) {
        zero_bit_count++;
        ConsumeBits(1);
    }
    // The bit count of the value is the number of zeros + 1.
    size_t value_bit_count = zero_bit_count + 1;
    // Make sure that many bits fits in a uint32_t and that we 
    // have enough bits left for it, and then read the value.
    if (value_bit_count > 32 || !ReadBits(value_bit_count, value)) {
        // Reset to original offset
        Seek(original_byte_offset, original_bit_offset);
        return false;
    }
    value -= 1;
    return true;
}

bool BitReader::ReadSignedExpGolomb(int32_t& value) {
    uint32_t unsigned_val;
    // unsigned: code_num = k.
    if (!ReadExpGolomb(unsigned_val)) {
        return false;
    }
    // signed:
    // code_num = 2|k| (k ≤ 0)
    // code_num = 2|k| − 1 (k > 0)
    // See https://www.jianshu.com/p/a31621affd40
    if ((unsigned_val & 1) == 0) {
        value = -static_cast<int32_t>(unsigned_val / 2);
    } else {
        value = (unsigned_val + 1) / 2;
    }
    return true;
}

bool BitReader::ConsumeBits(size_t bit_count) {
    if (bit_count > RemainingBitCount()) {
        return false;
    }
    size_t new_bit_offset = bit_offset_ + bit_count;
    byte_offset_ += new_bit_offset / 8;
    // in the range [0,7]
    bit_offset_ = new_bit_offset % 8;
    return true;
}

bool BitReader::Seek(size_t byte_offset, size_t bit_offset) {
    if (byte_offset > byte_count_ || 
        bit_offset > 7 ||
        (byte_offset == byte_count_ && bit_offset > 0)) {
        return false;
    }
    byte_offset_ = byte_offset;
    bit_offset_ = bit_offset;
    return true;
}

} // namespace naivertc