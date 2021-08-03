#include "rtc/rtp_rtcp/rtcp_packets/sender_report.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/common_header.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {

// See: https://datatracker.ietf.org/doc/html/rfc3550#section-6.4.1
/** 
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         SSRC of sender                        |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|              NTP timestamp, most significant word             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|             NTP timestamp, least significant word             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         RTP timestamp                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     sender's packet count                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      sender's octet count                     |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                      report blocks                            |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/

SenderReport::SenderReport() 
    : rtp_timestamp_(0), sender_packet_count_(0), sender_octet_count_(0) {}

SenderReport::SenderReport(const SenderReport&) = default;
SenderReport::SenderReport(SenderReport&&) = default;
SenderReport::~SenderReport() = default;
SenderReport& SenderReport::operator=(const SenderReport&) = default;
SenderReport& SenderReport::operator=(SenderReport&&) = default;

bool SenderReport::AddReportBlock(const ReportBlock& block) {
    if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
        PLOG_WARNING << "Max report blocks reached.";
        return false;
    }
    report_blocks_.push_back(block);
    return true;
}

bool SenderReport::SetReportBlocks(std::vector<ReportBlock> blocks) {
    if (blocks.size() > kMaxNumberOfReportBlocks) {
        PLOG_WARNING << "Too many report blocks (" << blocks.size() << ") for sender report.";
        return false;
    }
    report_blocks_ = std::move(blocks);
    return true;
}

bool SenderReport::Parse(const CommonHeader& packet) {
    if (packet.type() != kPacketType) {
        PLOG_WARNING << "Incoming packet is not a Sender Report.";
        return false;
    }
    const uint8_t report_block_count = packet.count();
    if (packet.payload_size() < kSenderReportFixedSize + report_block_count * ReportBlock::kFixedReportBlockSize) {
        PLOG_WARNING << "Packet is too small to contain all the data.";
        return false;
    }
    // Read SenderResport header
    const uint8_t* const payload = packet.payload();
    if (!payload) {
        PLOG_WARNING << "No payload data found in packet.";
        return false;
    }
    uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(&payload[0]);
    set_sender_ssrc(ssrc);
    uint32_t secs = ByteReader<uint32_t>::ReadBigEndian(&payload[4]);
    uint32_t frac = ByteReader<uint32_t>::ReadBigEndian(&payload[8]);
    ntp_.Set(secs, frac);
    rtp_timestamp_ = ByteReader<uint32_t>::ReadBigEndian(&payload[12]);
    sender_packet_count_ = ByteReader<uint32_t>::ReadBigEndian(&payload[16]);
    sender_octet_count_ = ByteReader<uint32_t>::ReadBigEndian(&payload[20]);
    
    const uint8_t* next_block = payload + kSenderReportFixedSize;
    for (ReportBlock& block : report_blocks_) {
        bool block_parsed = block.Parse(next_block, ReportBlock::kFixedReportBlockSize);
        if (!block_parsed) {
            PLOG_WARNING << "Failed to parse report block.";
            return false;   
        }
        next_block += ReportBlock::kFixedReportBlockSize;
    }

    // Double check we didn't read beyond provided buffer.
    assert(next_block - payload <= static_cast<ptrdiff_t>(packet.payload_size()));
    
    return true;
}

// Override RtcpPacket
size_t SenderReport::PacketSize() const {
    return kFixedRtcpCommonHeaderSize + kSenderReportFixedSize + report_blocks_.size() * ReportBlock::kFixedReportBlockSize;
}

bool SenderReport::PackInto(uint8_t* buffer,
                            size_t* index,
                            size_t max_size,
                            PacketReadyCallback callback) const {
    while (*index + PacketSize() > max_size) {
        if (!OnBufferFull(buffer, index, callback)) {
            return false;
        }
    }

    const size_t index_end = *index + PacketSize();
    
    RtcpPacket::PackCommonHeader(report_blocks_.size(), kPacketType, PacketSizeWithoutCommonHeader(), buffer, index);

    // Write SenderReport header
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index + 0], sender_ssrc());
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index + 4], ntp_.seconds());
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index + 8], ntp_.fractions());
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index + 12], rtp_timestamp_);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index + 16], sender_packet_count_);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index + 20], sender_octet_count_);

    *index += kSenderReportFixedSize;

    // Write report blocks
    for (const auto& block : report_blocks_) {
        if (!block.PackInto(buffer + *index, index_end - *index)) {
            PLOG_WARNING << "Too small space left in buffer to pack all of Report Blocks";
            return false;
        }
        *index += ReportBlock::kFixedReportBlockSize;
    }

    return true;
}

} // namespace rtcp
} // namespace naivertc
