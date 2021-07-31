#include "rtc/rtp_rtcp/rtcp_packets/common_header.hpp"
#include "rtc/base/byte_io_reader.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {

/* From RFC 3550, RTCP header format.
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |V=2|P| RC/FMT  |      PT       |             length            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

bool CommonHeader::ParseFrom(const uint8_t* buffer, size_t size) {
    constexpr uint8_t kVersion = 2;
    if (size < kFixedHeaderSize) {
        PLOG_WARNING << "Too little data remaining in buffer to parse RTCP header (4 bytes).";
        return false;
    }

    uint8_t version = buffer[0] >> 6;
    if (version != kVersion) {
        PLOG_WARNING << "invalid RTCP header: Version must be: " << kVersion << ", but was " << version;
        return false;
    }

    bool has_padding = (buffer[0] & 0x20) != 0;
    count_or_fmt_ = buffer[0] & 0x1F;
    packet_type_ = buffer[1];
    payload_size_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]) * 4;
    if (size < kFixedHeaderSize + payload_size_) {
        PLOG_WARNING << "Buffer too small to fit an RTCP packet with a header and " << payload_size_ << " bytes.";
        return false;
    }

    payload_ = buffer + kFixedHeaderSize;

    padding_size_ = 0;
    if (has_padding) {
        if (payload_size_ == 0) {
            PLOG_WARNING << "Invalid RTCP header: Padding bit set but payload size set to 0.";
            return false;
        }

        // the last byte in payload
        padding_size_  = payload_[payload_size_ - 1];
        if (padding_size_ == 0) {
            PLOG_WARNING << "Invalid RTCP header: Padding bit set but padding size set to 0.";
            return false;
        }

        if (padding_size_ > payload_size_) {
            PLOG_WARNING << "Invalid RTCP header: Too many padding bytes (" << padding_size_ << ") for a payload of " << payload_size_ << " bytes.";
            return false;
        }

        payload_size_  -= padding_size_;
    }

    return true;
}


} // namespace rtcp
} // namespace naivertc
