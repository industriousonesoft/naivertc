#include "rtc/rtp_rtcp/rtcp_packets/report_block.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {

// Report Block
/** 
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                 SSRC_1 (SSRC of first source)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| fraction lost |       cumulative number of packets lost       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           extended highest sequence number received           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      interarrival jitter                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         last SR (LSR)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   delay since last SR (DLSR)                  |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/

ReportBlock::ReportBlock() 
    : ssrc_(0),
    fraction_lost_(0),
    cumulative_packet_lost_(0),
    seq_num_cycles_(0),
    highest_seq_num_(0),
    jitter_(0),
    last_sr_ntp_timestamp_(0),
    delay_since_last_sr_(0) {}

ReportBlock::~ReportBlock() = default;

bool ReportBlock::PackInto(uint8_t* buffer, size_t size) const {
    if (size < kFixedReportBlockSize) {
        PLOG_WARNING << "Too small space left in buffer to pack report block (24 bytes)";
        return false;
    }
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[0], ssrc_);
    ByteWriter<uint8_t>::WriteBigEndian(&buffer[4], fraction_lost_);
    ByteWriter<int32_t, 3>::WriteBigEndian(&buffer[5], cumulative_packet_lost_);
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[8], seq_num_cycles_);
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[10], highest_seq_num_);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[12], jitter_);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[16], last_sr_ntp_timestamp_);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[20], delay_since_last_sr_);
    return true;
}

bool ReportBlock::ParseFrom(const uint8_t* buffer, size_t size) {
    assert(buffer != nullptr);
    if (size < kFixedReportBlockSize) {
        PLOG_WARNING << "Too little data remaining in buffer to parse Report Block (24 bytes).";
        return false;
    }

    ssrc_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[0]);
    fraction_lost_ = buffer[4];
    cumulative_packet_lost_ = ByteReader<int32_t, 3>::ReadBigEndian(&buffer[5]);
    seq_num_cycles_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[8]);
    highest_seq_num_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[10]);
    jitter_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[12]);
    last_sr_ntp_timestamp_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[16]);
    delay_since_last_sr_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[20]);

    return true;
}

bool ReportBlock::set_cumulative_packet_lost(int32_t cumulative_lost) {
    // It'a signed 24-bit value
    if (cumulative_lost >= (1 << 23) /* 0x800000 */ || cumulative_lost < -(1 << 23) /* -0x800000 */) {
        PLOG_WARNING << "Cumulative lost is too big to fit into Report Block.";
        return false;
    }
    cumulative_packet_lost_ = cumulative_lost;
    return true;
}

void ReportBlock::set_extended_highest_sequence_num(uint32_t extended_seq_num) {
    seq_num_cycles_ = extended_seq_num >> 16;
    highest_seq_num_ = extended_seq_num;
}
    
} // namespace rtcp
} // namespace naivert 
