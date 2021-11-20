#include "rtc/base/memory/bit_io_writer.hpp"

namespace naivertc {

BitWriter::BitWriter(uint8_t* bytes, size_t byte_count) 
    : bytes_(bytes),
      byte_count_(byte_count),
      byte_offset_(0),
      bit_offset_(0) {}

void BitWriter::GetCurrentOffset(size_t* out_byte_offset,
                                 size_t* out_bit_offset) {
    assert(out_byte_offset != nullptr);
    assert(out_bit_offset != nullptr);
    *out_byte_offset = byte_offset_;
    *out_bit_offset = bit_offset_;
}

uint64_t BitWriter::RemainingBitCount() const {
    return (static_cast<uint64_t>(byte_count_) - byte_offset_) * 8 - bit_offset_;
}

bool BitWriter::WriteBits(uint64_t val, size_t bit_count) {
    if (bit_count > RemainingBitCount()) {
        return false;
    }
    size_t total_bits = bit_count;
    // For simplicity, push the bits we want to read from val to the highest bits.
    val <<= (sizeof(uint64_t) * 8 - bit_count);
    uint8_t* bytes = bytes_ + byte_offset_;
    // The first byte is relatively special; the bit offset to write to may put us
    // in the middle of the byte, and the total bit count to write may require we
    // save the bits at the end of the byte.
    size_t remaining_bits_in_current_byte = 8 - bit_offset_;
    size_t bits_in_first_byte =  std::min(bit_count, remaining_bits_in_current_byte);
    *bytes = WritePartialByte(LeftMostByte(val), bits_in_first_byte, *bytes, bit_offset_);
    if (bit_count <= remaining_bits_in_current_byte) {
        // Nothing left to write, so quit early.
        return ConsumeBits(total_bits);
    }

    // Subtract what we've written from the bit count, shift it off the value, and
    // write the remaining full bytes.
    val <<= bits_in_first_byte;
    bytes++;
    bit_count -= bits_in_first_byte;
    while (bit_count >= 8) {
        *bytes++ = LeftMostByte(val);
        val <<= 8;
        bit_count -= 8;
    }

    // Last byte may also be partial, so write the remaining bits from the top of
    // val.
    if (bit_count > 0) {
        *bytes = WritePartialByte(LeftMostByte(val), bit_count, *bytes, 0);
    }

    // All done! Consume the bits we've written.
    return ConsumeBits(total_bits);
}

bool BitWriter::Seek(size_t byte_offset, size_t bit_offset) {
    if (byte_offset > byte_count_ || 
        bit_offset > 7 ||
        (byte_offset == byte_count_ && bit_offset > 0)) {
        return false;
    }
    byte_offset_ = byte_offset;
    bit_offset_ = bit_offset;
    return true;
}

bool BitWriter::WriteExpGolomb(uint32_t val) {
    // We don't support reading UINT32_MAX, because it doesn't fit in a uint32_t
    // when encoded, so don't support writing it either.
    if (val == std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    uint64_t val_to_encode = static_cast<uint64_t>(val) + 1;

    // We need to write CountBits(val+1) 0s and then val+1. Since val (as a
    // uint64_t) has leading zeros, we can just write the total golomb encoded
    // size worth of bits, knowing the value will appear last.
    return WriteBits(val_to_encode, CountBits(val_to_encode) * 2 - 1);
}

bool BitWriter::WriteSignedExpGolomb(int32_t val) {
    if (val == 0) {
        return WriteExpGolomb(0);
    } else if (val > 0) {
        uint32_t signed_val = val;
        return WriteExpGolomb((signed_val * 2) - 1);
    } else {
        if (val == std::numeric_limits<int32_t>::min())
        return false;  // Not supported, would cause overflow.
        uint32_t signed_val = -val;
        return WriteExpGolomb(signed_val * 2);
    }
}

// Private methods
bool BitWriter::ConsumeBits(size_t bit_count) {
    if (bit_count > RemainingBitCount()) {
        return false;
    }
    size_t new_bit_offset = bit_offset_ + bit_count;
    byte_offset_ += new_bit_offset / 8;
    // in the range [0,7]
    bit_offset_ = new_bit_offset % 8;
    return true;
}

// Returns the result of writing partial data from `source`, of
// `source_bit_count` size in the highest bits, to `target` at
// `target_bit_offset` from the highest bit.
uint8_t BitWriter::WritePartialByte(uint8_t source,
                                    size_t source_bit_count,
                                    uint8_t target,
                                    size_t target_bit_offset) {
    assert(target_bit_offset < 8);
    assert(source_bit_count < 9);
    assert(source_bit_count <= (8 - target_bit_offset));
    // Generate a mask for just the bits we're going to overwrite, so:
    uint8_t mask =
        // The number of bits we want, in the most significant bits...
        static_cast<uint8_t>(0xFF << (8 - source_bit_count))
        // ...shifted over to the target offset from the most signficant bit.
        >> target_bit_offset;

    // We want the target, with the bits we'll overwrite masked off, or'ed with
    // the bits from the source we want.
    return (target & ~mask) | (source >> target_bit_offset);
}

} // namespace naivertc
