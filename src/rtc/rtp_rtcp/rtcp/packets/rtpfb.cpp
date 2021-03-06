#include "rtc/rtp_rtcp/rtcp/packets/rtpfb.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {

// RFC 4585, Section 6.1: Feedback format.
//
// Common packet format:
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P|   FMT   |       PT      |          length               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 0 |                  SSRC of packet sender                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 4 |                  SSRC of media source                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   :            Feedback Control Information (FCI)                 :
//   :                                                               :


bool Rtpfb::ParseCommonFeedback(const uint8_t* buffer, size_t size) {
    if (size < kCommonFeedbackSize) {
        PLOG_WARNING << "Too little data remaining in buffer to parse Common Feedback (8 bytes)";
        return false;
    }
    set_sender_ssrc(ByteReader<uint32_t>::ReadBigEndian(&buffer[0]));
    set_media_ssrc(ByteReader<uint32_t>::ReadBigEndian(&buffer[4]));
    return true;
}

bool Rtpfb::PackCommonFeedbackInto(uint8_t* buffer, size_t size) const {
    if (size < kCommonFeedbackSize) {
        PLOG_WARNING << "Too small space left in buffer to pack Common Feedback (8 bytes)";
        return false;
    }
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[0], sender_ssrc());
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[4], media_ssrc());
    return true;
}

} // namespace rtcp
} // namespace naivertc
