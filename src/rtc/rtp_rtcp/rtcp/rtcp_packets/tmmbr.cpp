#include "rtc/rtp_rtcp/rtcp/rtcp_packets/tmmbr.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"

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
// Temporary Maximum Media Stream Bit Rate Request (TMMBR) (RFC 5104).
// The Feedback Control Information (FCI) for the TMMBR
// consists of one or more FCI entries.
// FCI:
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                              SSRC                             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | MxTBR Exp |  MxTBR Mantissa                 |Measured Overhead|
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Tmmbr::Tmmbr() = default;

Tmmbr::~Tmmbr() = default;

bool Tmmbr::Parse(const CommonHeader& packet) {
    if (packet.type() != kPacketType) 
        return false;
    if (packet.feedback_message_type() != kFeedbackMessageType) 
        return false;

    if (packet.payload_size() < kCommonFeedbackSize + TmmbItem::kFixedTmmbItemSize) {
        PLOG_WARNING << "Payload length " << packet.payload_size()
                     << " is too small for a TMMBR.";
        return false;
    }
    size_t items_size_bytes = packet.payload_size() - kCommonFeedbackSize;
    if (items_size_bytes % TmmbItem::kFixedTmmbItemSize != 0) {
        PLOG_WARNING << "Payload length " << packet.payload_size()
                     << " is not valid for a TMMBR.";
        return false;
    }
    ParseCommonFeedback(packet.payload(), packet.payload_size());

    const uint8_t* next_item = packet.payload() + kCommonFeedbackSize;
    size_t number_of_items = items_size_bytes / TmmbItem::kFixedTmmbItemSize;
    items_.resize(number_of_items);
    for (TmmbItem& item : items_) {
        if (!item.Parse(next_item))
        return false;
        next_item += TmmbItem::kFixedTmmbItemSize;
    }
    return true;
}

void Tmmbr::AddTmmbr(const TmmbItem& item) {
    items_.push_back(item);
}

size_t Tmmbr::PacketSize() const {
    return kRtcpCommonHeaderSize + kCommonFeedbackSize + TmmbItem::kFixedTmmbItemSize * items_.size();
}

bool Tmmbr::PackInto(uint8_t* buffer,
                   size_t* index,
                   size_t max_size,
                   PacketReadyCallback callback) const {
    if (items_.empty()) {
        return false;
    }
    while (*index + PacketSize() > max_size) {
        if (!OnBufferFull(buffer, index, callback))
        return false;
    }
    const size_t index_end = *index + PacketSize();

    RtcpPacket::PackCommonHeader(kFeedbackMessageType, kPacketType, PacketSizeWithoutCommonHeader(), buffer, index);
    if(0 != RtpFeedback::media_ssrc()) {
        return false;
    }
    RtpFeedback::PackCommonFeedbackInto(buffer + *index, index_end - *index);
    *index += kCommonFeedbackSize;
    for (const TmmbItem& item : items_) {
        item.PackInto(buffer + *index, index_end - *index);
        *index += TmmbItem::kFixedTmmbItemSize;
    }
    assert(index_end == *index);
    return true;
}
    
} // namespace rtcp
} // namespace naivertc
