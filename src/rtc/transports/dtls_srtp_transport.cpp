#include "rtc/transports/dtls_srtp_transport.hpp"
#include "rtc/rtp_rtcp/base/rtp_utils.hpp"

#include <plog/Log.h>

namespace naivertc {

DtlsSrtpTransport::DtlsSrtpTransport(DtlsTransport::Configuration config,
                                     bool is_client,
                                     BaseTransport* lower) 
    : DtlsTransport(std::move(config), is_client, lower),
      srtp_init_done_(false) {
    PLOG_DEBUG << "Initializing DTLS-SRTP transport";
    CreateSrtp();
}

DtlsSrtpTransport::~DtlsSrtpTransport() {
    RTC_RUN_ON(&sequence_checker_);
    DtlsTransport::Stop();
    DestroySrtp();
}

int DtlsSrtpTransport::SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options) {
    RTC_RUN_ON(&sequence_checker_);
    if (!packet.empty() && EncryptPacket(packet, false)) {
        return Outgoing(std::move(packet), std::move(options));
    } else {
        return -1;
    }
}

int DtlsSrtpTransport::SendRtcpPacket(CopyOnWriteBuffer packet, PacketOptions options) {
    RTC_RUN_ON(&sequence_checker_);
    if (!packet.empty() && EncryptPacket(packet, true)) {
        return Outgoing(std::move(packet), std::move(options));
    } else {
        return -1;
    }
}

void DtlsSrtpTransport::OnReceivedRtpPacket(RtpPacketRecvCallback callback) {
    RTC_RUN_ON(&sequence_checker_);
    rtp_packet_recv_callback_ = callback;
}

// Private methods
bool DtlsSrtpTransport::EncryptPacket(CopyOnWriteBuffer& packet, bool is_rtcp) {
    RTC_RUN_ON(&sequence_checker_);
    if (!srtp_init_done_) {
        PLOG_WARNING << "SRTP not init yet.";
        return false;
    }

    int protectd_data_size = (int)packet.size();
    // Rtcp packet
    if (is_rtcp && IsRtcpPacket(packet)) {
        // srtp_protect() and srtp_protect_rtcp() assume that they can write SRTP_MAX_TRAILER_LEN (for the authentication tag)
        // into the location in memory immediately following the RTP packet.
        size_t reserve_packet_size = protectd_data_size + SRTP_MAX_TRAILER_LEN /* 144 bytes defined in srtp.h */;
        packet.Resize(reserve_packet_size);

        if (srtp_err_status_t err = srtp_protect_rtcp(srtp_out_, packet.data(), &protectd_data_size)) {
            if (err == srtp_err_status_replay_fail) {
                throw std::runtime_error("Outgoing SRTCP packet is a replay");
            } else {
                throw std::runtime_error("SRTCP protect error, status=" +
                                            std::to_string(static_cast<int>(err)));
            }
        }
        PLOG_VERBOSE_IF(false) << "Protected SRTCP packet, size=" << protectd_data_size;

        packet.Resize(protectd_data_size);
    // Rtp packet
    } else if (!is_rtcp && IsRtpPacket(packet)) {
        // srtp_protect() and srtp_protect_rtcp() assume that they can write SRTP_MAX_TRAILER_LEN (for the authentication tag)
        // into the location in memory immediately following the RTP packet.
        size_t reserve_packet_size = protectd_data_size + SRTP_MAX_TRAILER_LEN /* 144 bytes defined in srtp.h */;
        packet.Resize(reserve_packet_size);

        if (srtp_err_status_t err = srtp_protect(srtp_out_, packet.data(), &protectd_data_size)) {
            if (err == srtp_err_status_replay_fail) {
                throw std::runtime_error("Outgoing SRTP packet is a replay");
            } else {
                throw std::runtime_error("SRTP protect error, status=" +
                                            std::to_string(static_cast<int>(err)));
            }
        }
        PLOG_VERBOSE_IF(false) << "Protected SRTP packet, size=" << protectd_data_size;

        packet.Resize(protectd_data_size);
    } else {
        PLOG_WARNING << "Sending packet is neither a RTP packet nor a RTCP packet, ignoring.";
        return false;
    }
    return true;
}

void DtlsSrtpTransport::DtlsHandshakeDone() {
    RTC_RUN_ON(&sequence_checker_);
    if (srtp_init_done_) {
        return;
    }
    InitSrtp();
    srtp_init_done_ = true;
}

void DtlsSrtpTransport::Incoming(CopyOnWriteBuffer in_packet) {
    RTC_RUN_ON(&sequence_checker_);
    // DTLS handshake is still in progress
    if (!srtp_init_done_) {
        DtlsTransport::Incoming(std::move(in_packet));
        return;
    }

    size_t packet_size = in_packet.size();
    if (packet_size == 0) {
        return;
    }

    // https://tools.ietf.org/html/rfc5764#section-5.1.2
    // The process for demultiplexing a packet is as follows. The receiver looks at the first byte
    // of the packet. [...] If the value is in between 128 and 191 (inclusive), then the packet is
    // RTP (or RTCP [...]). If the value is between 20 and 63 (inclusive), the packet is DTLS.
    uint8_t first_byte = in_packet.cdata()[0];

    PLOG_VERBOSE_IF(false) << "Demultiplexing DTLS and SRTP/SRTCP with first byte: " << std::to_string(first_byte);

    // DTLS packet
    if (first_byte >= 20 && first_byte <= 63) {
        DtlsTransport::Incoming(std::move(in_packet));
    // RTP/RTCP packet
    } else if (first_byte >= 128 && first_byte <= 191) {
        // RTCP packet
        if (IsRtcpPacket(in_packet)) {
            PLOG_VERBOSE_IF(false) << "Incoming SRTCP packet, size: " << packet_size;
            int unprotected_data_size = int(packet_size);
            if (srtp_err_status_t err = srtp_unprotect_rtcp(srtp_in_, static_cast<void *>(in_packet.data()), &unprotected_data_size)) {
                if (err == srtp_err_status_replay_fail) {
                    PLOG_VERBOSE << "Incoming SRTCP packet is a replay.";
                } else if (err == srtp_err_status_auth_fail) {
                    PLOG_VERBOSE << "Incoming SRTCP packet failed authentication check.";
                } else {
                    PLOG_VERBOSE << "SRTCP unprotect error, status: " << err;
                }
                return;
            }
            PLOG_VERBOSE_IF(false) << "Unprotected SRTCP packet, size: " << unprotected_data_size;
            
            in_packet.Resize(unprotected_data_size);

            if (rtp_packet_recv_callback_) {
                rtp_packet_recv_callback_(std::move(in_packet), true /* RTCP */);
            }
        // RTP packet
        } else if (IsRtpPacket(in_packet)) {
            PLOG_VERBOSE << "Incoming SRTP packet, size: " << packet_size;
            int unprotected_data_size = int(packet_size);
            if (srtp_err_status_t err = srtp_unprotect(srtp_in_, static_cast<void *>(in_packet.data()), &unprotected_data_size)) {
                if (err == srtp_err_status_replay_fail) {
                    PLOG_VERBOSE << "Incoming SRTP packet is a replay.";
                } else if (err == srtp_err_status_auth_fail) {
                    PLOG_VERBOSE << "Incoming SRTP packet failed authentication check.";
                } else {
                    PLOG_VERBOSE << "SRTCP unprotect error, status: " << err;
                }
                return;
            }
            PLOG_VERBOSE << "Unprotected SRTP packet, size: " << unprotected_data_size;
            
            in_packet.Resize(unprotected_data_size);

            if (rtp_packet_recv_callback_) {
                rtp_packet_recv_callback_(std::move(in_packet), false);
            }
            
        } else {
            PLOG_WARNING << "Incoming packet is neither a RTP packet nor a RTCP packet, ignoring.";
        }
    } else {
        PLOG_WARNING << "Incoming packet is neither a RTP/RTCP packet nor a DTLS packet, ignoring.";
    }
}

int DtlsSrtpTransport::Outgoing(CopyOnWriteBuffer out_packet, PacketOptions options) {
    RTC_RUN_ON(&sequence_checker_);
    // Set recommended medium-priority DSCP value
    // See https://datatracker.ietf.org/doc/html/draft-ietf-tsvwg-rtcweb-qos-18
    if (options.dscp == DSCP::DSCP_DF) {
        if (options.kind == PacketKind::AUDIO) {
            options.dscp = DSCP::DSCP_EF; // EF: Expedited Forwarding
        } else if (options.kind == PacketKind::VIDEO) {
            options.dscp = DSCP::DSCP_AF42; // AF42: Assured Forwarding class 4, medium drop probability
        }
    }
    return ForwardOutgoingPacket(std::move(out_packet), std::move(options));
}
    
} // namespace naivertc
