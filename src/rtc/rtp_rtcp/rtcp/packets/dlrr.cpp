#include "rtc/rtp_rtcp/rtcp/packets/dlrr.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"
#include "common/utils_numeric.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {
// DLRR Report Block (RFC 3611).
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |     BT=5      |   reserved    |         block length          |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |                 SSRC_1 (SSRC of first receiver)               | sub-
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
//  |                         last RR (LRR)                         |   1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                   delay since last RR (DLRR)                  |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |                 SSRC_2 (SSRC of second receiver)              | sub-
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
//  :                               ...                             :   2

Dlrr::Dlrr() = default;
Dlrr::Dlrr(const Dlrr& other) = default;
Dlrr::~Dlrr() = default;

bool Dlrr::Parse(const uint8_t* buffer, uint16_t block_length_32bits) {
    if (buffer[0] != kBlockType) return false;
    // kReserved = buffer[1];
    if (block_length_32bits != ByteReader<uint16_t>::ReadBigEndian(&buffer[2])) {
        return false;
    }
    if (block_length_32bits % 3 != 0) {
        PLOG_WARNING << "Invalid size for dlrr block.";
        return false;
    }

    size_t blocks_count = block_length_32bits / 3;
    const uint8_t* read_at = buffer + kBlockHeaderSize;
    sub_blocks_.resize(blocks_count);
    for (ReceiveTimeInfo& sub_block : sub_blocks_) {
        sub_block.ssrc = ByteReader<uint32_t>::ReadBigEndian(&read_at[0]);
        sub_block.last_rr = ByteReader<uint32_t>::ReadBigEndian(&read_at[4]);
        sub_block.delay_since_last_rr = ByteReader<uint32_t>::ReadBigEndian(&read_at[8]);
        read_at += kSubBlockSize;
    }
    return true;
}

size_t Dlrr::BlockSize() const {
    if (sub_blocks_.empty())
        return 0;
    return kBlockHeaderSize + kSubBlockSize * sub_blocks_.size();
}

void Dlrr::PackInto(uint8_t* buffer, size_t size) const {
    if (sub_blocks_.empty())  // No subblocks, no need to write header either.
        return;
    if (size < BlockSize()) {
        return;
    }
    // Create block header.
    const uint8_t kReserved = 0;
    buffer[0] = kBlockType;
    buffer[1] = kReserved;
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[2], utils::numeric::checked_static_cast<uint16_t>(3 * sub_blocks_.size()));
    // Create sub blocks.
    uint8_t* write_at = buffer + kBlockHeaderSize;
    for (const ReceiveTimeInfo& sub_block : sub_blocks_) {
        ByteWriter<uint32_t>::WriteBigEndian(&write_at[0], sub_block.ssrc);
        ByteWriter<uint32_t>::WriteBigEndian(&write_at[4], sub_block.last_rr);
        ByteWriter<uint32_t>::WriteBigEndian(&write_at[8], sub_block.delay_since_last_rr);
        write_at += kSubBlockSize;
    }
    assert(buffer + BlockSize() == write_at);
}
    
} // namespace rtcp
} // namespace naivertc
