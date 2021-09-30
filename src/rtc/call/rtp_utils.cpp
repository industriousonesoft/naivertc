#include "rtc/call/rtp_utils.hpp"

// #include <boost/range/irange.hpp>

namespace naivertc {
namespace {
constexpr uint8_t kFixedRtpVersion = 2;
// The RTP header has a minimum size of 12 bytes
constexpr size_t kFixedRtpPacketSize = 12;
// The RTCP header has a minimum size of 8 bytes
constexpr size_t kFixedRtcpPacketSize = 8;

// Payload type range
// RFC 5761 Multiplexing RTP and RTCP 4. Distinguishable RTP and RTCP Packets
// It is RECOMMENDED to follow the guidelines in the RTP/AVP profile for the choice of RTP
// payload type values, with the additional restriction that payload type values in the
// range 64-95 MUST NOT be used. Specifically, dynamic RTP payload types SHOULD be chosen in
// the range 96-127 where possible. Values below 64 MAY be used if that is insufficient.
// Rang 64-95 (inclusive) MUST be RTCP
// For detail: https://tools.ietf.org/html/rfc5761#section-4
constexpr uint8_t kRtcpPayloadTypeLowerRangeValue = 64;
constexpr uint8_t kRtcpPayloadTypeUpperRangeValue = 95;

constexpr uint8_t kRtpPayloadTypeLowerRangeValue = 96;
constexpr uint8_t kRtpPayloadTypeUpperRangeValue = 127;

bool CheckRtpVersion(ArrayView<const uint8_t> packet) {
    return kFixedRtpVersion == packet[0] >> 6;
}

bool PayloadTypeIsReservedForRtcp(uint8_t payload_type) {
    return payload_type >= kRtcpPayloadTypeLowerRangeValue &&
           payload_type <= kRtcpPayloadTypeUpperRangeValue;
}

bool PayloadTypeIsReservedForRtp(uint8_t payload_type) {
    return payload_type >= kRtpPayloadTypeLowerRangeValue && 
           payload_type <= kRtpPayloadTypeUpperRangeValue;
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

} // namespace naivertc