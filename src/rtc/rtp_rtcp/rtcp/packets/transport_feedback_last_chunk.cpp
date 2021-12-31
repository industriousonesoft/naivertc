#include "rtc/rtp_rtcp/rtcp/packets/transport_feedback.hpp"

namespace naivertc {
namespace rtcp {

// LastChunk
constexpr size_t TransportFeedback::LastChunk::kMaxRunLengthCapacity;
constexpr size_t TransportFeedback::LastChunk::kMaxOneBitCapacity;
constexpr size_t TransportFeedback::LastChunk::kMaxTwoBitCapacity;
constexpr size_t TransportFeedback::LastChunk::kMaxVectorCapacity;

TransportFeedback::LastChunk::LastChunk() {
    Clear();
}

bool TransportFeedback::LastChunk::Empty() const {
    return size_ == 0;
}

void TransportFeedback::LastChunk::Clear() {
    size_ = 0;
    all_same_ = true;
    has_large_delta_ = false;
}

bool TransportFeedback::LastChunk::CanAdd(DeltaSize delta_size) const {
    assert(delta_size <= 2);
    if (size_ < kMaxTwoBitCapacity)
        return true;
    if (size_ < kMaxOneBitCapacity && !has_large_delta_ && delta_size != kLarge)
        return true;
    if (size_ < kMaxRunLengthCapacity && all_same_ &&
        delta_sizes_[0] == delta_size)
        return true;
    return false;
}

void TransportFeedback::LastChunk::Add(DeltaSize delta_size) {
    assert(CanAdd(delta_size));
    if (size_ < kMaxVectorCapacity)
        delta_sizes_[size_] = delta_size;
    size_++;
    all_same_ = all_same_ && delta_size == delta_sizes_[0];
    has_large_delta_ = has_large_delta_ || delta_size == kLarge;
}

uint16_t TransportFeedback::LastChunk::Emit() {
    assert(!CanAdd(0) || !CanAdd(1) || !CanAdd(2));
    if (all_same_) {
        uint16_t chunk = EncodeRunLength();
        Clear();
        return chunk;
    }
    if (size_ == kMaxOneBitCapacity) {
        uint16_t chunk = EncodeOneBit();
        Clear();
        return chunk;
    }
    assert(size_ >= kMaxTwoBitCapacity);
    uint16_t chunk = EncodeTwoBit(kMaxTwoBitCapacity);
    // Remove |kMaxTwoBitCapacity| encoded delta sizes:
    // Shift remaining delta sizes and recalculate all_same_ && has_large_delta_.
    size_ -= kMaxTwoBitCapacity;
    all_same_ = true;
    has_large_delta_ = false;
    for (size_t i = 0; i < size_; ++i) {
        DeltaSize delta_size = delta_sizes_[kMaxTwoBitCapacity + i];
        delta_sizes_[i] = delta_size;
        all_same_ = all_same_ && delta_size == delta_sizes_[0];
        has_large_delta_ = has_large_delta_ || delta_size == kLarge;
    }
    return chunk;
}

uint16_t TransportFeedback::LastChunk::EncodeLast() const {
    assert(size_ > 0);
    if (all_same_)
        return EncodeRunLength();
    if (size_ <= kMaxTwoBitCapacity)
        return EncodeTwoBit(size_);
    return EncodeOneBit();
}

// Appends content of the Lastchunk to |deltas|.
void TransportFeedback::LastChunk::AppendTo(std::vector<DeltaSize>* deltas) const {
    if (all_same_) {
        deltas->insert(deltas->end(), size_, delta_sizes_[0]);
    } else {
        deltas->insert(deltas->end(), delta_sizes_, delta_sizes_ + size_);
    }
}

void TransportFeedback::LastChunk::Decode(uint16_t chunk, size_t max_size) {
    if ((chunk & 0x8000) == 0) {
        DecodeRunLength(chunk, max_size);
    } else if ((chunk & 0x4000) == 0) {
        DecodeOneBit(chunk, max_size);
    } else {
        DecodeTwoBit(chunk, max_size);
    }
}

//  One Bit Status Vector Chunk
//
//  0                   1
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |T|S|       symbol list         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  T = 1
//  S = 0
//  Symbol list = 14 entries where 0 = not received, 1 = received 1-byte delta.
uint16_t TransportFeedback::LastChunk::EncodeOneBit() const {
    assert(!has_large_delta_);
    assert(size_ <= kMaxOneBitCapacity);
    uint16_t chunk = 0x8000;
    for (size_t i = 0; i < size_; ++i)
        chunk |= delta_sizes_[i] << (kMaxOneBitCapacity - 1 - i);
    return chunk;
}

void TransportFeedback::LastChunk::DecodeOneBit(uint16_t chunk,
                                                size_t max_size) {
    assert((chunk & 0xc000) == 0x8000);
    size_ = std::min(kMaxOneBitCapacity, max_size);
    has_large_delta_ = false;
    all_same_ = false;
    for (size_t i = 0; i < size_; ++i) {
        delta_sizes_[i] = (chunk >> (kMaxOneBitCapacity - 1 - i)) & 0x01;
    }
}

//  Two Bit Status Vector Chunk
//
//  0                   1
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |T|S|       symbol list         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  T = 1
//  S = 1
//  symbol list = 7 entries of two bits each.
uint16_t TransportFeedback::LastChunk::EncodeTwoBit(size_t size) const {
    assert(size <= size_);
    uint16_t chunk = 0xc000;
    for (size_t i = 0; i < size; ++i)
        chunk |= delta_sizes_[i] << 2 * (kMaxTwoBitCapacity - 1 - i);
    return chunk;
}

void TransportFeedback::LastChunk::DecodeTwoBit(uint16_t chunk,
                                                size_t max_size) {
    assert((chunk & 0xc000) == 0xc000);
    size_ = std::min(kMaxTwoBitCapacity, max_size);
    has_large_delta_ = true;
    all_same_ = false;
    for (size_t i = 0; i < size_; ++i) {
        delta_sizes_[i] = (chunk >> 2 * (kMaxTwoBitCapacity - 1 - i)) & 0x03;
    }
}

//  Run Length Status Vector Chunk
//
//  0                   1
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |T| S |       Run Length        |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  T = 0
//  S = symbol
//  Run Length = Unsigned integer denoting the run length of the symbol
uint16_t TransportFeedback::LastChunk::EncodeRunLength() const {
    assert(all_same_);
    assert(size_ <= kMaxRunLengthCapacity);
    return (delta_sizes_[0] << 13) | static_cast<uint16_t>(size_);
}

void TransportFeedback::LastChunk::DecodeRunLength(uint16_t chunk,
                                                   size_t max_count) {
    assert((chunk & 0x8000) == 0);
    size_ = std::min<size_t>(chunk & 0x1fff, max_count);
    DeltaSize delta_size = (chunk >> 13) & 0x03;
    has_large_delta_ = delta_size >= kLarge;
    all_same_ = true;
    // To make it consistent with Add function, populate delta_sizes_ beyound 1st.
    for (size_t i = 0; i < std::min<size_t>(size_, kMaxVectorCapacity); ++i) {
        delta_sizes_[i] = delta_size;
    } 
}
    
} // namespace rtcp
} // namespace naivert 
