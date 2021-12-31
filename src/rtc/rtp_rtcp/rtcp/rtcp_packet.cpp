#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <plog/Log.h>

namespace naivertc {

CopyOnWriteBuffer RtcpPacket::Build() const {
    CopyOnWriteBuffer packet(PacketSize());

    size_t size = 0;
    bool created = PackInto(packet.data(), &size, packet.capacity(), nullptr);
    if (!created) {
        PLOG_WARNING << "Failed to packet RTCP packet.";
        packet.Resize(0);
    }
    assert(size == packet.size() && "PacketSize mispredicted size used.");
    return packet;
}

bool RtcpPacket::Build(size_t max_size, PacketReadyCallback callback) const {
    assert(max_size <= kIpPacketSize);
    uint8_t buffer[kIpPacketSize];
    size_t index = 0;
    if (!PackInto(buffer, &index, max_size, callback)) {
        return false;
    }
    return OnBufferFull(buffer, &index, callback);
}

bool RtcpPacket::OnBufferFull(uint8_t* buffer, size_t* index, PacketReadyCallback callback) const {
    if (*index == 0) {
        return false;
    }
    if (callback == nullptr) {
        PLOG_WARNING << "Fragment not supported.";
        return false;
    }
    callback(CopyOnWriteBuffer(buffer, buffer + *index));
    *index = 0;
    return true;
}

size_t RtcpPacket::PacketSizeWithoutCommonHeader() const {
    size_t length_in_bytes = PacketSize();
    assert(length_in_bytes > 0);
    assert(length_in_bytes % 4 == 0 && "Padding must be handled by each subclass.");
    // Length in 32-bit words without common header
    return (length_in_bytes - kRtcpCommonHeaderSize);
}

// From RFC 3550, RTCP header format.
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P| RC/FMT  |      PT       |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// PT: payload type, RFC3550 Section-12.1
// abbrev.  name                 value
// SR       sender report          200
// RR       receiver report        201
// SDES     source description     202
// BYE      goodbye                203
// APP      application-defined    204
// ...

void RtcpPacket::PackCommonHeader(
        size_t count_or_format,
        uint8_t packet_type,
        size_t payload_size,
        uint8_t* buffer,
        size_t* index) {
    PackCommonHeader(count_or_format, packet_type, payload_size, /* padding=*/false , buffer, index);
}

void RtcpPacket::PackCommonHeader(
        size_t count_or_format,
        uint8_t packet_type,
        size_t payload_size,
        bool padding,
        uint8_t* buffer,
        size_t* index) {
    assert(payload_size <= 0xFFFFU);
    assert(count_or_format <= 0x1F);
    constexpr uint8_t kVersionBits = 2 << 6;
    uint8_t padding_bit = padding ? 1 << 5 : 0;
    // Payload size saved in 32-bit word
    size_t payload_size_in_32bit = payload_size / 4;
    buffer[*index + 0] = kVersionBits | padding_bit | static_cast<uint8_t>(count_or_format);
    buffer[*index + 1] = packet_type;
    buffer[*index + 2] = (payload_size_in_32bit >> 8) & 0xFF;
    buffer[*index + 3] = payload_size_in_32bit & 0xFF;
    *index += kRtcpCommonHeaderSize;
}
    
} // namespace naivertc
