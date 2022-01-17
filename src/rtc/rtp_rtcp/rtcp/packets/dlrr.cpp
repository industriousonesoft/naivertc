#include "rtc/rtp_rtcp/rtcp/packets/dlrr.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"
#include "common/utils_numeric.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {

// TimeInfo
Dlrr::TimeInfo::TimeInfo() 
    : ssrc(0), 
      last_rr(0), 
      delay_since_last_rr(0) {}

Dlrr::TimeInfo::TimeInfo(uint32_t ssrc, 
                         uint32_t last_rr, 
                         uint32_t delay)
    : ssrc(ssrc), 
      last_rr(last_rr), 
      delay_since_last_rr(delay) {}

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

bool Dlrr::Parse(const uint8_t* buffer, size_t size) {
    if (size < kBlockHeaderSize || buffer[0] != kBlockType) {
        return false;
    }
    // kReserved = buffer[1];
    uint16_t items_count = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);
    if (items_count % 3 != 0) {
        PLOG_WARNING << "Invalid item count for dlrr block.";
        return false;
    }

    size_t block_size = kBlockHeaderSize + items_count * 4;
    if (block_size > size) {
        PLOG_WARNING << "Invalid size for dlrr block.";
        return false;
    }
    
    size_t time_info_count = items_count / /* Every subblock has 3 items.*/3;
    const uint8_t* read_at = buffer + kBlockHeaderSize;
    time_infos_.resize(time_info_count);
    for (auto& time_info : time_infos_) {
        time_info.ssrc = ByteReader<uint32_t>::ReadBigEndian(&read_at[0]);
        time_info.last_rr = ByteReader<uint32_t>::ReadBigEndian(&read_at[4]);
        time_info.delay_since_last_rr = ByteReader<uint32_t>::ReadBigEndian(&read_at[8]);
        read_at += kTimeInfoSize;
    }
    return true;
}

size_t Dlrr::BlockSize() const {
    if (time_infos_.empty())
        return 0;
    return kBlockHeaderSize + kTimeInfoSize * time_infos_.size();
}

void Dlrr::PackInto(uint8_t* buffer, size_t size) const {
    if (time_infos_.empty())  // No subblocks, no need to write header either.
        return;
    if (size < BlockSize()) {
        return;
    }
    // Create block header.
    const uint8_t kReserved = 0;
    buffer[0] = kBlockType;
    buffer[1] = kReserved;
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[2], utils::numeric::checked_static_cast<uint16_t>(3 * time_infos_.size()));
    // Create sub blocks.
    uint8_t* write_at = buffer + kBlockHeaderSize;
    for (const auto& time_info : time_infos_) {
        ByteWriter<uint32_t>::WriteBigEndian(&write_at[0], time_info.ssrc);
        ByteWriter<uint32_t>::WriteBigEndian(&write_at[4], time_info.last_rr);
        ByteWriter<uint32_t>::WriteBigEndian(&write_at[8], time_info.delay_since_last_rr);
        write_at += kTimeInfoSize;
    }
    assert(buffer + BlockSize() == write_at);
}
    
} // namespace rtcp
} // namespace naivertc
