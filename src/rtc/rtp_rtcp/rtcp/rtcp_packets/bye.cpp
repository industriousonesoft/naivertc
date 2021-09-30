#include "rtc/rtp_rtcp/rtcp/rtcp_packets/bye.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {

// Bye packet (BYE) (RFC 3550).
//
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |V=2|P|    SC   |   PT=BYE=203  |             length            |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |                           SSRC/CSRC                           |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       :                              ...                              :
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// (opt) |     length    |               reason for leaving            ...
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Bye::Bye() = default;

Bye::~Bye() = default;

bool Bye::Parse(const CommonHeader& packet) {
    if(packet.type() != kPacketType) {
        return false;
    }

    const uint8_t src_count = packet.count();
    // Validate packet.
    if (packet.payload_size() < 4u * src_count) {
        PLOG_WARNING << "Packet is too small to contain CSRCs it promise to have.";
        return false;
    }
    const uint8_t* const payload = packet.payload();
    bool has_reason = packet.payload_size() > 4u * src_count;
    uint8_t reason_length = 0;
    if (has_reason) {
        reason_length = payload[4u * src_count];
        if (packet.payload_size() - 4u * src_count < 1u + reason_length) {
            PLOG_WARNING << "Invalid reason length: " << reason_length;
            return false;
        }
    }
    // Once sure packet is valid, copy values.
    if (src_count == 0) {  // A count value of zero is valid, but useless.
        set_sender_ssrc(0);
        csrcs_.clear();
    } else {
        set_sender_ssrc(ByteReader<uint32_t>::ReadBigEndian(payload));
        csrcs_.resize(src_count - 1);
        for (size_t i = 1; i < src_count; ++i) {
            csrcs_[i - 1] = ByteReader<uint32_t>::ReadBigEndian(&payload[4 * i]);
        }
    }

    if (has_reason) {
        reason_.assign(reinterpret_cast<const char*>(&payload[4u * src_count + 1]),
                    reason_length);
    } else {
        reason_.clear();
    }

    return true;
}


bool Bye::PackInto(uint8_t* buffer,
                   size_t* index,
                   size_t max_size,
                   PacketReadyCallback callback) const {
    while (*index + PacketSize() > max_size) {
        if (!OnBufferFull(buffer, index, callback))
        return false;
    }
    const size_t index_end = *index + PacketSize();

    RtcpPacket::PackCommonHeader(1 + csrcs_.size(), kPacketType, PacketSizeWithoutCommonHeader(), buffer, index);
    // Store srcs of the leaving clients.
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index], sender_ssrc());
    *index += sizeof(uint32_t);
    for (uint32_t csrc : csrcs_) {
        ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index], csrc);
        *index += sizeof(uint32_t);
    }
    // Store the reason to leave.
    if (!reason_.empty()) {
        uint8_t reason_length = static_cast<uint8_t>(reason_.size());
        buffer[(*index)++] = reason_length;
        memcpy(&buffer[*index], reason_.data(), reason_length);
        *index += reason_length;
        // Add padding bytes if needed.
        size_t bytes_to_pad = index_end - *index;
        assert(bytes_to_pad <= 3);
        if (bytes_to_pad > 0) {
        memset(&buffer[*index], 0, bytes_to_pad);
        *index += bytes_to_pad;
        }
    }
    assert(index_end == *index);
    return true;
}

bool Bye::set_csrcs(std::vector<uint32_t> csrcs) {
    if (csrcs.size() > kMaxNumberOfCsrcs) {
        PLOG_WARNING << "Too many CSRCs for Bye packet.";
        return false;
    }
    csrcs_ = std::move(csrcs);
    return true;
}

void Bye::set_reason(std::string reason) {
    assert(reason.size() <= 0xffu);
    reason_ = std::move(reason);
}

size_t Bye::PacketSize() const {
    size_t src_count = (1 + csrcs_.size());
    size_t reason_size_in_32bits = reason_.empty() ? 0 : (reason_.size() / 4 + 1);
    return kRtcpCommonHeaderSize + 4 * (src_count + reason_size_in_32bits);
}
    
} // namespace rtcp    
} // namespace naivertc
