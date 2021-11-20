#include "rtc/rtp_rtcp/rtcp/rtcp_packets/receiver_report.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {

// RTCP receiver report (RFC 3550).
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|    RC   |   PT=RR=201   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     SSRC of packet sender                     |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |                         report block(s)                       |
//  |                            ....                               |

ReceiverReport::ReceiverReport() = default;

ReceiverReport::ReceiverReport(const ReceiverReport& other) = default;

ReceiverReport::~ReceiverReport() = default;

bool ReceiverReport::Parse(const CommonHeader& packet) {
    if (packet.type() != kPacketType) {
        PLOG_WARNING << "Incoming packet is not a Receiver Report.";
        return false;
    }
    const uint8_t report_block_count = packet.count();
    if (packet.payload_size() < kReceiverReportBaseSize + report_block_count * ReportBlock::kFixedReportBlockSize) {
        PLOG_WARNING << "Packet is too small to contain all the data.";
        return false;
    }
    const uint8_t* payload_buffer = packet.payload();
    if (!payload_buffer) {
        PLOG_WARNING << "Packet contains a empty payload.";
        return false;
    }
    set_sender_ssrc(ByteReader<uint32_t>::ReadBigEndian(payload_buffer));

    const uint8_t* next_report_block = payload_buffer + kReceiverReportBaseSize;
    report_blocks_.resize(report_block_count);
    for (auto &block : report_blocks_) {
        block.Parse(next_report_block, ReportBlock::kFixedReportBlockSize);
        next_report_block += ReportBlock::kFixedReportBlockSize;
    }

    assert(next_report_block - payload_buffer <= static_cast<ptrdiff_t>(packet.payload_size()));

    return true;
}

size_t ReceiverReport::PacketSize() const {
    return kRtcpCommonHeaderSize + kReceiverReportBaseSize + report_blocks_.size() * ReportBlock::kFixedReportBlockSize;
}

bool ReceiverReport::PackInto(uint8_t* buffer,
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

    ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index], sender_ssrc());
    *index += kReceiverReportBaseSize;
    for (const ReportBlock& block : report_blocks_) {
        block.PackInto(&buffer[*index], index_end - *index);
        *index += ReportBlock::kFixedReportBlockSize;
    }
    return true;
}

bool ReceiverReport::AddReportBlock(const ReportBlock& block) {
    if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
        PLOG_WARNING << "Max report blocks reached.";
        return false;
    }
    report_blocks_.push_back(block);
    return true;
}

bool ReceiverReport::SetReportBlocks(std::vector<ReportBlock> blocks) {
    if (blocks.size() > kMaxNumberOfReportBlocks) {
        PLOG_WARNING << "Too many report blocks ("
                     << blocks.size()
                     << ") for receiver report.";
        return false;
    }
    report_blocks_ = std::move(blocks);
    return true;
}
    
} // namespace rtcp
} // namespace naivertc
