#include "rtc/rtp_rtcp/common/rtp_utils.hpp"

namespace naivertc {
namespace rtp {
namespace utils {
namespace {
constexpr uint8_t kFixedRtpVersion = 2;
// The RTP header has a minimum size of 12 bytes
constexpr size_t kFixedRtpPacketSize = 12;
// The RTCP header has a minimum size of 8 bytes
constexpr size_t kFixedRtcpPacketSize = 8;

bool CheckRtpVersion(ArrayView<const uint8_t> packet) {
    return kFixedRtpVersion == packet[0] >> 6;
}

// RFC 5761 Multiplexing RTP and RTCP 4. Distinguishable RTP and RTCP Packets
// It is RECOMMENDED to follow the guidelines in the RTP/AVP profile for the choice of RTP
// payload type values, with the additional restriction that payload type values in the
// range 64-95 MUST NOT be used. Specifically, dynamic RTP payload types SHOULD be chosen in
// the range 96-127 where possible. Values below 64 MAY be used if that is insufficient.
// Rang 64-95 (inclusive) MUST be RTCP
// For detail: https://tools.ietf.org/html/rfc5761#section-4
bool PayloadTypeIsReservedForRtcp(uint8_t payload_type) {
    return payload_type >= 64 && payload_type <= 95;
}

bool PayloadTypeIsReservedForRtp(uint8_t payload_type) {
    return payload_type >= 96 && payload_type <= 127;
}

} // namespace

bool IsRtcpPacket(ArrayView<const uint8_t> packet) {
    return packet.size() >= kFixedRtcpPacketSize && 
           CheckRtpVersion(packet) &&
           PayloadTypeIsReservedForRtcp(packet[1] & 0x7F);
}

bool IsRtpPacket(ArrayView<const uint8_t> packet) {
    return packet.size() >= kFixedRtpPacketSize && 
           CheckRtpVersion(packet) &&
           PayloadTypeIsReservedForRtp(packet[1] & 0x7F);
}

} // namespace utils
} // namespace rtp
} // namespace naivertc