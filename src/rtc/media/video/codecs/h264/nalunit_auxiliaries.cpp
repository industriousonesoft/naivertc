#include "rtc/media/video/codecs/h264/nalunit.hpp"

namespace naivertc {
namespace h264 {

std::vector<NaluIndex> NalUnit::FindNaluIndices(const uint8_t* buffer, size_t size) {
    // This is sorta like Boyer-Moore, but with only the first optimization step:
    // given a 3-byte sequence we are looking at, if the 3rd byte isn't 1 or 0, skip
    // ahead to the next 3-byte sequence. 0 and 1 are relatively rate, so this will
    // skip the majority of reads/checks
    std::vector<NaluIndex> indices;
    if (size < kNaluShortStartSequenceSize) {
        return indices;
    }
    const size_t end = size - kNaluShortStartSequenceSize;
    for (size_t i = 0; i < end;) {
        if (buffer[i + 2] > 1) {
            i += 3;
        }else if (buffer[i + 2] == 1) {
            // Found a start sequence, now check if it was a 3 or 4 byte one.
            if (buffer[i + 1] == 0 && buffer[i] == 0) {
                NaluIndex index = {i, i + 3, 0};
                // If the index is not the first one, and the byte in front of the start offet is 0
                if (index.start_offset > 0 && buffer[index.start_offset - 1] == 0) {
                    --index.start_offset;
                }

                // Update length of previous nalu index
                auto it = indices.rbegin();
                if (it != indices.rend()) {
                    it->payload_size = index.start_offset - it->payload_start_offset;
                }

                indices.push_back(index);
            }
            i += 3;
        }else {
            ++i;
        }
    }

    // Update length of the last index, if any.
    auto it = indices.rbegin();
    if (it != indices.rend()) {
        it->payload_size = size - it->payload_start_offset;
    }
    return indices;
}

    
} // namespace H264
} // namespace naivertc
