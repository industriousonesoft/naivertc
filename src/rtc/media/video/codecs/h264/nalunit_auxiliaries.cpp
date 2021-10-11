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

std::vector<uint8_t> NalUnit::RetrieveRbspFromEbsp(const uint8_t* ebsp_buffer, size_t ebsp_size) {
    std::vector<uint8_t> rbsp_buffer;
    rbsp_buffer.reserve(ebsp_size);
    for (size_t i = 0; i < ebsp_size;) {
        if (ebsp_size - i >= 3 && 
            ebsp_buffer[i] == 0x00 && 
            ebsp_buffer[i + 1] == 0x00 && 
            ebsp_buffer[i + 2] == 0x03) {
            // Two RBSP bytes
            rbsp_buffer.push_back(ebsp_buffer[i++]);
            rbsp_buffer.push_back(ebsp_buffer[i++]);
            // Skip the emulation byte.
            i++;
        }else {
            // Single RBSP byte.
            rbsp_buffer.push_back(ebsp_buffer[i++]);
        }
    }
    return rbsp_buffer;
}

void NalUnit::WriteRbsp(const uint8_t* rbsp_buffer, size_t rbsp_size, std::vector<uint8_t>& ebsp_buffer) {
    static const uint8_t kZerosInStartSequence = 2;
    static const uint8_t kEmulationByte = 0x03u;
    size_t num_consecutive_zeros = 0;
    ebsp_buffer.reserve(ebsp_buffer.size() + rbsp_size);

    for (size_t i = 0; i < rbsp_size; ++i) {
        uint8_t byte = rbsp_buffer[i];
        // Insert emulation byte to escape.
        if (byte <= kEmulationByte &&
            num_consecutive_zeros >= kZerosInStartSequence) {
            ebsp_buffer.push_back(kEmulationByte);
            num_consecutive_zeros = 0;
        }
        ebsp_buffer.push_back(byte);
        if (byte == 0) {
            ++num_consecutive_zeros;
        } else {
            num_consecutive_zeros = 0;
        }
    }
}
    
} // namespace H264
} // namespace naivertc
