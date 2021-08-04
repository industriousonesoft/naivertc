#include "rtc/rtp_rtcp/rtcp_packets/pli.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/common_header.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {
// RFC 4585: Feedback format.
//
// Common packet format:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|   FMT   |       PT      |          length               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  SSRC of packet sender                        |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  SSRC of media source                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :            Feedback Control Information (FCI)                 :
//  :                                                               :

Pli::Pli() = default;
Pli::Pli(const Pli&) = default;
Pli::~Pli() = default;

// No feedback control information (FCI) in PLI
bool Pli::Parse(const CommonHeader& packet) {
    if (packet.type() != Psfb::kPacketType) {
        PLOG_WARNING << "Incoming packet is not a Payload-Specific feedback packet.";
        return false;
    }
    if (packet.feedback_message_type() != kFeedbackMessageType) {
        PLOG_WARNING << "Incoming packet is not a Picture losss indication packet.";
        return false;
    }
    if (packet.payload_size() < kCommonFeedbackSize) {
        PLOG_WARNING << "Packet is too small to be a valid PLI packet.";
        return false;
    }
    Psfb::ParseCommonFeedback(packet.payload());
    return true;
}   

size_t Pli::PacketSize() const {
    return kRtcpCommonHeaderSize + kCommonFeedbackSize;
}

bool Pli::PackInto(uint8_t* buffer,
              size_t* index,
              size_t max_size,
              PacketReadyCallback callback) const {
    while (*index + PacketSize() > max_size) {
        if (!OnBufferFull(buffer, index, callback)) {
            return false;
        }
    }
    RtcpPacket::PackCommonHeader(kFeedbackMessageType, kPacketType, PacketSizeWithoutCommonHeader(), buffer, index);
    Psfb::PackCommonFeedback(&buffer[*index]);
    *index += kCommonFeedbackSize;
    return true;    
}
    
} // namespace rtcp
} // namespace naivert 
