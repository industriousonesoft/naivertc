#include "rtc/rtp_rtcp/rtcp/rtcp_packets/fir.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {
// RFC 4585: Feedback format.
// Common packet format:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|   FMT   |       PT      |          length               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  SSRC of packet sender                        |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |             SSRC of media source (unused) = 0                 |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :            Feedback Control Information (FCI)                 :
//  :                                                               :
// Full intra request (FIR) (RFC 5104).
// The Feedback Control Information (FCI) for the Full Intra Request
// consists of one or more FCI entries.
// FCI:
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                              SSRC                             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | Seq nr.       |    Reserved = 0                               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Fir::Fir() = default;
Fir::Fir(const Fir&) = default;
Fir::~Fir() = default;

bool Fir::Parse(const CommonHeader& packet) {
    if (packet.type() != Psfb::kPacketType) {
        PLOG_WARNING << "Incoming packet is not a Payload-specific feedback packet.";
        return false;
    }
    if (packet.feedback_message_type() != Fir::kFeedbackMessageType) {
        PLOG_WARNING << "Incoming packet is not a Full intra request packet.";
        return false;
    }
    if (packet.payload_size() < Psfb::kCommonFeedbackSize + kFciSize) {
        PLOG_WARNING << "Invalid size for a valid FIR packet.";
        return false;
    }

    if ((packet.payload_size() - kCommonFeedbackSize) % kFciSize != 0) {
        PLOG_WARNING << "Invalid size for a valid FIR packet.";
        return false;
    }

    Psfb::ParseCommonFeedback(packet.payload());

    size_t number_of_fci_items = (packet.payload_size() - kCommonFeedbackSize) / kFciSize;
    fci_itmes_.clear();
    fci_itmes_.resize(number_of_fci_items);
    const uint8_t* next_fci = packet.payload() + kCommonFeedbackSize;
    for (auto& request : fci_itmes_) {
        request.ssrc = ByteReader<uint32_t>::ReadBigEndian(next_fci);
        request.seq_nr = ByteReader<uint8_t>::ReadBigEndian(next_fci + 4);
        next_fci += kFciSize;
    }
    return true;
}

size_t Fir::PacketSize() const {
    return RtcpPacket::kRtcpCommonHeaderSize + Psfb::kCommonFeedbackSize + Fir::kFciSize * fci_itmes_.size();
}

bool Fir::PackInto(uint8_t* buffer,
                size_t* index,
                size_t max_size,
                PacketReadyCallback callback) const {
    if (fci_itmes_.empty()) {
        PLOG_WARNING << "No fci items in FIR packet.";
        return false;
    }
    while (*index + PacketSize() > max_size) {
        if (!OnBufferFull(buffer, index, callback)) {
            return false;
        }
    }
    if (Psfb::media_ssrc() != 0) {
        PLOG_WARNING << "Media ssrc unused in FIR packet is supposed to be zero.";
        return false;
    }
    
    size_t index_end = *index + PacketSize();
    // RTCP common header
    RtcpPacket::PackCommonHeader(kFeedbackMessageType, kPacketType, PacketSizeWithoutCommonHeader(), buffer, index);
    // Payload-specific feedback common fields
    Psfb::PackCommonFeedback(&buffer[*index]);
    *index += kCommonFeedbackSize;

    constexpr uint32_t kReserved = 0;
    for (const auto& fci : fci_itmes_) {
        ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index], fci.ssrc);
        ByteWriter<uint8_t>::WriteBigEndian(&buffer[*index + 4], fci.seq_nr);
        ByteWriter<uint32_t, 3>::WriteBigEndian(&buffer[*index + 5], kReserved);
        *index += kFciSize;
    }

    assert(*index == index_end);

    return true;
}
    
} // namespace rtcp
} // namespace naviertc
