#include "rtc/rtp_rtcp/rtcp/packets/extended_reports.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/common_header.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {
constexpr size_t ExtendedReports::kMaxNumberOfDlrrSubBlocks;
// From RFC 3611: RTP Control Protocol Extended Reports (RTCP XR).
//
// Format for XR packets:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|reserved |   PT=XR=207   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                              SSRC                             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :                         report blocks                         :
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// Extended report block:
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  Block Type   |   reserved    |         block length          |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :             type-specific block contents                      :
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

ExtendedReports::ExtendedReports() {}
    
ExtendedReports::~ExtendedReports() {}

const std::optional<Rrtr>& ExtendedReports::rrtr() const {
    return rrtr_block_;
}

const Dlrr& ExtendedReports::dlrr() const {
    return dlrr_block_;
}

const std::optional<TargetBitrate>& ExtendedReports::target_bitrate() const {
    return target_bitrate_;
}

void ExtendedReports::set_rrtr(const Rrtr& rrtr) {
    rrtr_block_.emplace(rrtr);
}

bool ExtendedReports::AddDlrrSubBlock(const Dlrr::SubBlock& block) {
    if (dlrr_block_.sub_blocks().size() >= kMaxNumberOfDlrrSubBlocks) {
        PLOG_WARNING << "Reached maximum number of DLRR sub blocks.";
        return false;
    }
    dlrr_block_.AddDlrrSubBlock(block);
    return true;
}

void ExtendedReports::set_target_bitrate(const TargetBitrate& bitrate) {
    target_bitrate_.emplace(bitrate);
}

bool ExtendedReports::Parse(const CommonHeader& packet) {
    if (packet.type() != kPacketType) {
        return false;
    }

    if (packet.payload_size() < kXrBaseSize) {
        PLOG_WARNING << "Packet is too small to be an ExtendedReports packet.";
        return false;
    }

    // ssrc
    set_sender_ssrc(ByteReader<uint32_t>::ReadBigEndian(packet.payload()));
    rrtr_block_.reset();
    dlrr_block_.Clear();
    target_bitrate_.reset();

    const uint8_t* curr_block = packet.payload() + kXrBaseSize;
    const uint8_t* const packet_end = packet.payload() + packet.payload_size();
    const size_t kBlockHeaderSize = 4;
    while (curr_block + kBlockHeaderSize <= packet_end) {
        uint8_t block_type = curr_block[0];
        // uint8_t reserved = curr_block[1]
        uint16_t block_size_words = ByteReader<uint16_t>::ReadBigEndian(curr_block + 2);
        size_t block_size = kBlockHeaderSize + block_size_words * 4;
        const uint8_t* next_block = curr_block + block_size;
        if (next_block > packet_end) {
            PLOG_WARNING << "Report block in ExtendedReport packet is too big.";
            return false;
        }
        switch (block_type)
        {
        case Rrtr::kBlockType:
            ParseRrtrBlock(curr_block, block_size);
            break;
        case Dlrr::kBlockType:
            ParseDlrrBlock(curr_block, block_size);
            break;
        case TargetBitrate::kBlockType:
            ParseTaragetBitrateBlock(curr_block, block_size);
            break;
        default:
            PLOG_WARNING << "Unknown extended report block type=" << block_type;
            break;
        }
        curr_block = next_block;
    }

    return true;
}

size_t ExtendedReports::PacketSize() const {
    return kRtcpCommonHeaderSize + kXrBaseSize + RrtrBlockSize() + DlrrBlockSize() + TargetBitrateBlockSize();
}

bool ExtendedReports::PackInto(uint8_t* buffer,
                               size_t* index,
                               size_t max_size,
                               PacketReadyCallback callback) const {
    while (*index + PacketSize() > max_size) {
        if (OnBufferFull(buffer, index, callback)) {
            return false;
        }
    }

    size_t index_end = *index + PacketSize();
    const uint8_t kReserved = 0;
    RtcpPacket::PackCommonHeader(kReserved, kPacketType, PacketSizeWithoutCommonHeader(), buffer, index);
    ByteWriter<uint32_t>::WriteBigEndian(buffer + *index, sender_ssrc());
    *index += sizeof(uint32_t);
    if (rrtr_block_) {
        rrtr_block_->PackInto(buffer + *index, index_end - *index);
        *index += rrtr_block_->BlockSize();
    }
    if (dlrr_block_) {
        dlrr_block_.PackInto(buffer + *index, index_end - *index);
        *index += dlrr_block_.BlockSize();
    }
    if (target_bitrate_) {
        target_bitrate_->PackInto(buffer + *index, index_end - *index);
        *index += target_bitrate_->BlockSize();
    }
    assert(*index == index_end);                        
    return true;
}

// Private methods
size_t ExtendedReports::RrtrBlockSize() const {
    return rrtr_block_ ? rrtr_block_->BlockSize() : 0;
}

size_t ExtendedReports::DlrrBlockSize() const {
    return dlrr_block_.BlockSize();
}

size_t ExtendedReports::TargetBitrateBlockSize() const {
    return target_bitrate_ ? target_bitrate_->BlockSize() : 0;
}

void ExtendedReports::ParseRrtrBlock(const uint8_t* buffer, size_t size) {
    rrtr_block_.emplace();
    if (!rrtr_block_->Parse(buffer, size)) {
        PLOG_WARNING << "No rrtr block found in the extended report packet.";
    }
}

void ExtendedReports::ParseDlrrBlock(const uint8_t* buffer, size_t size) {
    if (!dlrr_block_.Parse(buffer, size)) {
        PLOG_WARNING << "No dlrr block found in the extended report packet.";
    }
}

void ExtendedReports::ParseTaragetBitrateBlock(const uint8_t* buffer, size_t size) {
    target_bitrate_.emplace();
    if (!target_bitrate_->Parse(buffer, size)) {
        PLOG_WARNING << "No target bitrate block found in the extended report packet.";
    }
}


} // namespace rtcp
} // namespace naivertc