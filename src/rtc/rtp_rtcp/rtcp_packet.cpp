#include "rtc/rtp_rtcp/rtcp_packet.hpp"

namespace naivertc {

bool RtcpPacket::Build(size_t max_length, PacketReadyCallback callback) const {
    assert(max_length <= kDefaultPacketSize);
    uint8_t buffer[kDefaultPacketSize];
    size_t index = 0;
    if (!Create(buffer, &index, max_length, callback)) {
        return false;
    }
    return OnBufferFull(buffer, &index, callback);
}

bool RtcpPacket::OnBufferFull(uint8_t* packet, size_t* index, PacketReadyCallback callback) const {
    if (*index == 0) {
        return false;
    }
    // TODO: Call callback
    // callback()
    return true;
}

size_t RtcpPacket::HeaderLength() const {
    size_t length_in_bytes = BlockLength();
    assert(length_in_bytes > 0);
    assert(length_in_bytes % 4 == 0 && "Padding must be handled by each subclass.");
    // Length in 32-bit words without common header
    return (length_in_bytes - kHeaderLength) / 4;
}

/* From RFC 3550, RTCP header format.
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |V=2|P| RC/FMT  |      PT       |             length            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

void RtcpPacket::CreateHeader(
        size_t count_or_format,
        uint8_t packet_type,
        size_t block_length,
        uint8_t* buffer,
        size_t* pos) {
    CreateHeader(count_or_format, packet_type, block_length, /* padding=*/false , buffer, pos);
}

void RtcpPacket::CreateHeader(
        size_t count_or_format,
        uint8_t packet_type,
        size_t block_length,
        bool padding,
        uint8_t* buffer,
        size_t* pos) {
    assert(block_length <= 0xFFFFU);
    assert(count_or_format <= 0x1F);
    constexpr uint8_t kVersionBits = 2 << 6;
    uint8_t padding_bit = padding ? 1 << 5 : 0;
    buffer[*pos + 0] = kVersionBits | padding_bit | static_cast<uint8_t>(count_or_format);
    buffer[*pos + 1] = packet_type;
    buffer[*pos + 2] = (block_length >> 8) & 0xFF;
    buffer[*pos + 3] = block_length & 0xFF;
    *pos += kHeaderLength;
}
    
} // namespace naivertc
