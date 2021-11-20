#include "rtc/rtp_rtcp/rtcp/rtcp_packets/psfb.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

namespace naivertc {
namespace rtcp {

// RFC 4585: Feedback format.
//
// Common packet format:
//
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

void Psfb::ParseCommonFeedback(const uint8_t* payload) {
    set_sender_ssrc(ByteReader<uint32_t>::ReadBigEndian(&payload[0]));
    set_media_ssrc(ByteReader<uint32_t>::ReadBigEndian(&payload[4]));
}

void Psfb::PackCommonFeedback(uint8_t* payload) const {
    ByteWriter<uint32_t>::WriteBigEndian(&payload[0], sender_ssrc());
    ByteWriter<uint32_t>::WriteBigEndian(&payload[4], media_ssrc());
}
    
} // namespace rtcp
} // namespace naivertc
