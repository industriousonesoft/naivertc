#ifndef _TESTING_RTCP_PACKET_PARSER_H_
#define _TESTING_RTCP_PACKET_PARSER_H_

#include "rtc/rtp_rtcp/rtcp/packets/bye.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/common_header.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/fir.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/loss_notification.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/nack.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/pli.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/receiver_report.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/remb.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/sdes.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/sender_report.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/tmmbn.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/tmmbr.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/transport_feedback.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/extended_reports.hpp"
#include "common/array_view.hpp"

namespace naivertc {
namespace test {

// Parse RTCP packet of given type. Assumes RTCP header is valid and that there
// is excatly one packet of correct type in the buffer.
template <typename Packet>
bool ParseSinglePacket(const uint8_t* buffer, size_t size, Packet* packet) {
    rtcp::CommonHeader header;
    assert(header.Parse(buffer, size));
    assert(size == (header.NextPacket() - buffer));
    return packet->Parse(header);
}
// Same function, but takes raw buffer as single argument instead of pair.
template <typename Packet>
bool ParseSinglePacket(ArrayView<const uint8_t> buffer, Packet* packet) {
    return ParseSinglePacket(buffer.data(), buffer.size(), packet);
}

// RtcpPacketParser
class RtcpPacketParser {
public:
    // Keeps last parsed packet, count number of parsed packets of given type.
    template <typename TypedRtcpPacket>
    class PacketCounter : public TypedRtcpPacket {
    public:
        int num_packets() const { return num_packets_; }
        void Parse(const rtcp::CommonHeader& header) {
        if (TypedRtcpPacket::Parse(header))
            ++num_packets_;
        }
        bool Parse(const rtcp::CommonHeader& header, uint32_t* sender_ssrc) {
        const bool result = TypedRtcpPacket::Parse(header);
        if (result) {
            ++num_packets_;
            if (*sender_ssrc == 0)  // Use first sender ssrc in compound packet.
            *sender_ssrc = TypedRtcpPacket::sender_ssrc();
        }
        return result;
        }

    private:
        int num_packets_ = 0;
    };

    RtcpPacketParser();
    ~RtcpPacketParser();

    bool Parse(const void* packet, size_t packet_len);

    // PacketCounter<rtcp::App>* app() { return &app_; }
    PacketCounter<rtcp::Bye>* bye() { return &bye_; }
    // PacketCounter<rtcp::ExtendedJitterReport>* ij() { return &ij_; }
    PacketCounter<rtcp::ExtendedReports>* xr() { return &xr_; }
    PacketCounter<rtcp::Fir>* fir() { return &fir_; }
    PacketCounter<rtcp::Nack>* nack() { return &nack_; }
    PacketCounter<rtcp::Pli>* pli() { return &pli_; }
    // PacketCounter<rtcp::RapidResyncRequest>* rrr() { return &rrr_; }
    PacketCounter<rtcp::ReceiverReport>* receiver_report() {
        return &receiver_report_;
    }
    PacketCounter<rtcp::LossNotification>* loss_notification() {
        return &loss_notification_;
    }
    PacketCounter<rtcp::Remb>* remb() { return &remb_; }
    PacketCounter<rtcp::Sdes>* sdes() { return &sdes_; }
    PacketCounter<rtcp::SenderReport>* sender_report() { return &sender_report_; }
    PacketCounter<rtcp::Tmmbn>* tmmbn() { return &tmmbn_; }
    PacketCounter<rtcp::Tmmbr>* tmmbr() { return &tmmbr_; }
    PacketCounter<rtcp::TransportFeedback>* transport_feedback() {
        return &transport_feedback_;
    }
    uint32_t sender_ssrc() const { return sender_ssrc_; }
    size_t processed_rtcp_packets() const { return processed_rtcp_packets_; }

private:
    // PacketCounter<rtcp::App> app_;
    PacketCounter<rtcp::Bye> bye_;
    // PacketCounter<rtcp::ExtendedJitterReport> ij_;
    PacketCounter<rtcp::ExtendedReports> xr_;
    PacketCounter<rtcp::Fir> fir_;
    PacketCounter<rtcp::Nack> nack_;
    PacketCounter<rtcp::Pli> pli_;
    // PacketCounter<rtcp::RapidResyncRequest> rrr_;
    PacketCounter<rtcp::ReceiverReport> receiver_report_;
    PacketCounter<rtcp::LossNotification> loss_notification_;
    PacketCounter<rtcp::Remb> remb_;
    PacketCounter<rtcp::Sdes> sdes_;
    PacketCounter<rtcp::SenderReport> sender_report_;
    PacketCounter<rtcp::Tmmbn> tmmbn_;
    PacketCounter<rtcp::Tmmbr> tmmbr_;
    PacketCounter<rtcp::TransportFeedback> transport_feedback_;
    uint32_t sender_ssrc_ = 0;
    size_t processed_rtcp_packets_ = 0;
};
    
} // namespace test    
} // namespace naivertc


#endif