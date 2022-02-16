#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"

#include <plog/Log.h>

namespace naivertc {
// From RFC 3550, RTCP header format.
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P| RC/FMT  |      PT       |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// PT: payload type, RFC3550 Section-12.1
/*
* abbrev   name                                 value
*
* SR       sender report                        200     [RFC3551]   supported
* RR       receiver report                      201     [RFC3551]   supported
* SDES     source description                   202     [RFC3551]   supported
* BYE      goodbye                              203     [RFC3551]   supported
* APP      application-defined                  204     [RFC3551]   ignored
* RTPFB    Transport layer FB message           205     [RFC4585]   supported
* PSFB     Payload-specific FB message          206     [RFC4585]   supported
* XR       extended report                      207     [RFC3611]   supported
*/

/* 205       RFC 5104
* FMT 1      NACK       supported
* FMT 2      reserved
* FMT 3      TMMBR      supported
* FMT 4      TMMBN      supported
*/

/* 206       RFC 5104
* FMT 1:     Picture Loss Indication (PLI)                      supported
* FMT 2:     Slice Lost Indication (SLI)
* FMT 3:     Reference Picture Selection Indication (RPSI)
* FMT 4:     Full Intra Request (FIR) Command                   supported
* FMT 5:     Temporal-Spatial Trade-off Request (TSTR)
* FMT 6:     Temporal-Spatial Trade-off Notification (TSTN)
* FMT 7:     Video Back Channel Message (VBCM)
* FMT 15:    Application layer FB message
*/

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
