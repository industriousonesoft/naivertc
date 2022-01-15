#include "rtc/rtp_rtcp/rtcp/rtcp_packet_parser.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace test {

RtcpPacketParser::RtcpPacketParser() = default;
RtcpPacketParser::~RtcpPacketParser() = default;

bool RtcpPacketParser::Parse(const void* data, size_t length) {
    ++processed_rtcp_packets_;

    const uint8_t* const buffer = static_cast<const uint8_t*>(data);
    const uint8_t* const buffer_end = buffer + length;

    rtcp::CommonHeader header;
    for (const uint8_t* next_packet = buffer; next_packet != buffer_end;
        next_packet = header.NextPacket()) {
        assert((buffer_end - next_packet) > 0);
        if (!header.Parse(next_packet, buffer_end - next_packet)) {
            PLOG_WARNING << "Invalid rtcp header or unaligned rtcp packet at position "
                        << (next_packet - buffer);
            return false;
        }
        switch (header.type()) {
        // case rtcp::App::kPacketType:
        //     app_.Parse(header);
        //     break;
        case rtcp::Bye::kPacketType:
            bye_.Parse(header, &sender_ssrc_);
            break;
        case rtcp::ExtendedReports::kPacketType:
            xr_.Parse(header, &sender_ssrc_);
            break;
        // case rtcp::ExtendedJitterReport::kPacketType:
        //     ij_.Parse(header);
        //     break;
        case rtcp::Psfb::kPacketType:
            switch (header.feedback_message_type()) {
            case rtcp::Fir::kFeedbackMessageType:
                fir_.Parse(header, &sender_ssrc_);
                break;
            case rtcp::Pli::kFeedbackMessageType:
                pli_.Parse(header, &sender_ssrc_);
                break;
            case rtcp::Psfb::kAfbMessageType:
                if (!loss_notification_.Parse(header, &sender_ssrc_) &&
                    !remb_.Parse(header, &sender_ssrc_)) {
                    PLOG_WARNING << "Unknown application layer FB message.";
                }
                break;
            default:
                PLOG_WARNING << "Unknown rtcp payload specific feedback type "
                             << header.feedback_message_type();
                break;
            }
            break;
        case rtcp::ReceiverReport::kPacketType:
            receiver_report_.Parse(header, &sender_ssrc_);
            break;
        case rtcp::Rtpfb::kPacketType:
            switch (header.feedback_message_type()) {
            case rtcp::Nack::kFeedbackMessageType:
                nack_.Parse(header, &sender_ssrc_);
                break;
            // case rtcp::RapidResyncRequest::kFeedbackMessageType:
            //     rrr_.Parse(header, &sender_ssrc_);
            //     break;
            case rtcp::Tmmbn::kFeedbackMessageType:
                tmmbn_.Parse(header, &sender_ssrc_);
                break;
            case rtcp::Tmmbr::kFeedbackMessageType:
                tmmbr_.Parse(header, &sender_ssrc_);
                break;
            case rtcp::TransportFeedback::kFeedbackMessageType:
                transport_feedback_.Parse(header, &sender_ssrc_);
                break;
            default:
                PLOG_WARNING << "Unknown rtcp transport feedback type " << header.feedback_message_type();
                break;
            }
            break;
        case rtcp::Sdes::kPacketType:
            sdes_.Parse(header);
            break;
        case rtcp::SenderReport::kPacketType:
            sender_report_.Parse(header, &sender_ssrc_);
            break;
        default:
            PLOG_WARNING << "Unknown rtcp packet type " << header.type();
            break;
        }
    }
    return true;
}

} // namespace test    
} // namespace naivertc