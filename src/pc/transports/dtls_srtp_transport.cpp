#include "pc/transports/dtls_srtp_transport.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

namespace naivertc {

DtlsSrtpTransport::DtlsSrtpTransport(std::shared_ptr<IceTransport> lower, const DtlsTransport::Config& config) 
    : DtlsTransport(lower, std::move(config)),
    srtp_init_done_(false) {}

DtlsSrtpTransport::~DtlsSrtpTransport() {
    DtlsTransport::Stop();
    DeinitSrtp();
}

void DtlsSrtpTransport::HandshakeDone() {
    if (!srtp_init_done_) {
        InitSrtp();
        srtp_init_done_ = true;
    }
}

void DtlsSrtpTransport::Incoming(std::shared_ptr<Packet> in_packet) {
    // DTLS handshake is still in progress
    if (!srtp_init_done_) {
        DtlsTransport::Incoming(in_packet);
        return;
    }

    size_t packet_size = in_packet->size();
    if (packet_size == 0) {
        return;
    }

    // RFC 5764 5.1.2. Reception
	// https://tools.ietf.org/html/rfc5764#section-5.1.2
	// The process for demultiplexing a packet is as follows. The receiver looks at the first byte
	// of the packet. [...] If the value is in between 128 and 191 (inclusive), then the packet is
	// RTP (or RTCP [...]). If the value is between 20 and 63 (inclusive), the packet is DTLS.
    uint8_t first_byte = std::to_integer<uint8_t>(in_packet->bytes().front());
    PLOG_VERBOSE << "Demultiplexing DTLS and SRTP/SRTCP with firt byte: " << first_byte;

    // DTLS packet
    if (first_byte >= 20 && first_byte <= 63) {
        DtlsTransport::Incoming(in_packet);
    // RTP/RTCP packet
    }else if (first_byte >= 128 && first_byte <= 191) {
        // The RTP header has a minimum size of 12 bytes
        // The RTCP header has a minimum size of 8 bytes
        if (packet_size < 8) {
            PLOG_VERBOSE << "Incoming SRTP/SRTCP packet too small, size: " << packet_size;
            return;
        }

        uint8_t payload_type = std::to_integer<uint8_t>(in_packet->bytes()[1]) & 0x7F;
        PLOG_VERBOSE << "Demultiplexing SRTP and SRTCP with RTP payload type: " << payload_type;

        // RTCP packet: Range [64,95] 
        if (payload_type >= 64 && payload_type <= 95) {
            PLOG_VERBOSE << "Incoming SRTCP packet, size: " << packet_size;
            int unprotected_rtcp_packet_size = int(packet_size);
            if (srtp_err_status_t err = srtp_unprotect_rtcp(srtp_in_, static_cast<void *>(in_packet->data()), &unprotected_rtcp_packet_size)) {
                if (err == srtp_err_status_replay_fail) {
                    PLOG_VERBOSE << "Incoming SRTCP packet is a replay.";
                }else if (err == srtp_err_status_auth_fail) {
                    PLOG_VERBOSE << "Incoming SRTCP packet failed authentication check.";
                }else {
                    PLOG_VERBOSE << "SRTCP unprotect error, status: " << err;
                }
                return;
            }
            PLOG_VERBOSE << "Unprotected SRTCP packet, size: " << unprotected_rtcp_packet_size;
            // TODO: To get rtcp sr ssrc
            unsigned int ssrc = 0;
            in_packet->Resize(unprotected_rtcp_packet_size);
            auto rtcp_packet = RtpPacket::Create(std::move(in_packet), RtpPacket::Type::RTCP, ssrc);
            rtp_packet_recv_callback_(rtcp_packet);
        }else {
            PLOG_VERBOSE << "Incoming SRTP packet, size: " << packet_size;
            int unprotected_rtp_packet_size = int(packet_size);
            if (srtp_err_status_t err = srtp_unprotect(srtp_in_, static_cast<void *>(in_packet->data()), &unprotected_rtp_packet_size)) {
                if (err == srtp_err_status_replay_fail) {
                    PLOG_VERBOSE << "Incoming SRTP packet is a replay.";
                }else if (err == srtp_err_status_auth_fail) {
                    PLOG_VERBOSE << "Incoming SRTP packet failed authentication check.";
                }else {
                    PLOG_VERBOSE << "SRTCP unprotect error, status: " << err;
                }
                return;
            }
            PLOG_VERBOSE << "Unprotected SRTP packet, size: " << unprotected_rtp_packet_size;
            // TODO: To get rtp ssrc
            unsigned int ssrc = 0;
            // shrink size
            in_packet->Resize(unprotected_rtp_packet_size);
            auto rtcp_packet = RtpPacket::Create(std::move(in_packet), RtpPacket::Type::RTP, ssrc);
            rtp_packet_recv_callback_(rtcp_packet);
        }
    }else {
        PLOG_VERBOSE <<  "Unknown packet type, value: " << first_byte << ", size: " << packet_size;
    }
}

void DtlsSrtpTransport::Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) {

}

void DtlsSrtpTransport::OnReceivedRtpPacket(RtpPacketRecvCallback callback) {
    task_queue_.Post([this, callback](){
        rtp_packet_recv_callback_ = callback;
    });
}
    
} // namespace naivertc
