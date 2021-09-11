#include "rtc/transports/dtls_srtp_transport.hpp"
#include "rtc/rtp_rtcp/common/rtp_utils.hpp"

#include <plog/Log.h>

namespace naivertc {

DtlsSrtpTransport::DtlsSrtpTransport(const DtlsTransport::Configuration config, std::shared_ptr<IceTransport> lower, std::shared_ptr<TaskQueue> task_queue) 
    : DtlsTransport(std::move(config), std::move(lower), std::move(task_queue)),
    srtp_init_done_(false) {
    PLOG_DEBUG << "Initializing DTLS-SRTP transport";
    CreateSrtp();
}

DtlsSrtpTransport::~DtlsSrtpTransport() {
    DtlsTransport::Stop();
    DestroySrtp();
}

void DtlsSrtpTransport::DtlsHandshakeDone() {
    if (srtp_init_done_) {
        return;
    }
    InitSrtp();
    srtp_init_done_ = true;
}

void DtlsSrtpTransport::SendRtpPacket(std::shared_ptr<RtpPacket> packet, PacketSentCallback callback) {
    task_queue_->Async([this, packet=std::move(packet), callback](){
        if (packet && EncryptPacket(*packet.get())) {
            ForwardOutgoingPacket(std::move(packet), callback);
        }else {
            callback(-1);
        }
    });
}

int DtlsSrtpTransport::SendRtpPacket(std::shared_ptr<RtpPacket> packet) {
    return task_queue_->Sync<int>([this, packet=std::move(packet)](){
        if (packet && EncryptPacket(*packet.get())) {
            return ForwardOutgoingPacket(std::move(packet));
        }else {
            return -1;
        }
    });
}

bool DtlsSrtpTransport::EncryptPacket(Packet& packet) {
    if (!srtp_init_done_) {
        PLOG_WARNING << "SRTP not init yet.";
        return false;
    }

    int protectd_data_size = (int)packet.size();
    // Rtcp packet
    if (rtp::utils::IsRtcpPacket(packet)) {
        // srtp_protect() and srtp_protect_rtcp() assume that they can write SRTP_MAX_TRAILER_LEN (for the authentication tag)
        // into the location in memory immediately following the RTP packet.
        packet.resize(protectd_data_size + SRTP_MAX_TRAILER_LEN /* 144 bytes defined in srtp.h */);

        if (srtp_err_status_t err = srtp_protect_rtcp(srtp_out_, packet.data(), &protectd_data_size)) {
            if (err == srtp_err_status_replay_fail) {
                throw std::runtime_error("Outgoing SRTCP packet is a replay");
            }else {
                throw std::runtime_error("SRTCP protect error, status=" +
                                            std::to_string(static_cast<int>(err)));
            }
        }
        PLOG_VERBOSE << "Protected SRTCP packet, size=" << protectd_data_size;
    // Rtp packet
    }else if (rtp::utils::IsRtpPacket(packet)) {
        // srtp_protect() and srtp_protect_rtcp() assume that they can write SRTP_MAX_TRAILER_LEN (for the authentication tag)
        // into the location in memory immediately following the RTP packet.
        packet.resize(protectd_data_size + SRTP_MAX_TRAILER_LEN /* 144 bytes defined in srtp.h */);

        if (srtp_err_status_t err = srtp_protect(srtp_out_, packet.data(), &protectd_data_size)) {
            if (err == srtp_err_status_replay_fail) {
                throw std::runtime_error("Outgoing SRTP packet is a replay");
            }else {
                throw std::runtime_error("SRTP protect error, status=" +
                                            std::to_string(static_cast<int>(err)));
            }
        }
        PLOG_VERBOSE << "Protected SRTP packet, size=" << protectd_data_size;
    }else {
        PLOG_WARNING << "Sending packet is neither a RTP packet nor a RTCP packet, ignoring.";
        return false;
    }

    packet.resize(protectd_data_size);

    if (packet.dscp() == 0) {
        // Set recommended medium-priority DSCP value
        // See https://tools.ietf.org/html/draft-ietf-tsvwg-rtcweb-qos-18
        // AF42: Assured Forwarding class 4, medium drop probability
        packet.set_dscp(36);
    }

    return true;
}

void DtlsSrtpTransport::OnReceivedRtpPacket(RtpPacketRecvCallback callback) {
    task_queue_->Async([this, callback](){
        this->rtp_packet_recv_callback_ = callback;
    });
}

void DtlsSrtpTransport::Incoming(std::shared_ptr<Packet> in_packet) {
    task_queue_->Async([this, in_packet=std::move(in_packet)](){
        // DTLS handshake is still in progress
        if (!srtp_init_done_) {
            DtlsTransport::Incoming(in_packet);
            return;
        }

        size_t packet_size = in_packet->size();
        if (packet_size == 0) {
            return;
        }

        // https://tools.ietf.org/html/rfc5764#section-5.1.2
        // The process for demultiplexing a packet is as follows. The receiver looks at the first byte
        // of the packet. [...] If the value is in between 128 and 191 (inclusive), then the packet is
        // RTP (or RTCP [...]). If the value is between 20 and 63 (inclusive), the packet is DTLS.
        uint8_t first_byte = in_packet->data()[0];
        PLOG_VERBOSE << "Demultiplexing DTLS and SRTP/SRTCP with first byte: " << std::to_string(first_byte);

        // DTLS packet
        if (first_byte >= 20 && first_byte <= 63) {
            DtlsTransport::Incoming(in_packet);
        // RTP/RTCP packet
        }else if (first_byte >= 128 && first_byte <= 191) {
            // RTCP packet
            if (rtp::utils::IsRtcpPacket(*in_packet.get())) {
                PLOG_VERBOSE << "Incoming SRTCP packet, size: " << packet_size;
                int unprotected_data_size = int(packet_size);
                if (srtp_err_status_t err = srtp_unprotect_rtcp(srtp_in_, static_cast<void *>(in_packet->data()), &unprotected_data_size)) {
                    if (err == srtp_err_status_replay_fail) {
                        PLOG_VERBOSE << "Incoming SRTCP packet is a replay.";
                    }else if (err == srtp_err_status_auth_fail) {
                        PLOG_VERBOSE << "Incoming SRTCP packet failed authentication check.";
                    }else {
                        PLOG_VERBOSE << "SRTCP unprotect error, status: " << err;
                    }
                    return;
                }
                PLOG_VERBOSE << "Unprotected SRTCP packet, size: " << unprotected_data_size;
                // TODO: To parse rtcp sr and get ssrc
                // unsigned int ssrc = 0;
                in_packet->resize(unprotected_data_size);
                // auto rtcp_packet = RtpPacket::Create(std::move(in_packet), RtpPacket::Type::RTCP, ssrc);
                // if (rtp_packet_recv_callback_) {
                //     rtp_packet_recv_callback_(rtcp_packet);
                // }
            }else if (rtp::utils::IsRtpPacket(*in_packet.get())) {
                PLOG_VERBOSE << "Incoming SRTP packet, size: " << packet_size;
                int unprotected_data_size = int(packet_size);
                if (srtp_err_status_t err = srtp_unprotect(srtp_in_, static_cast<void *>(in_packet->data()), &unprotected_data_size)) {
                    if (err == srtp_err_status_replay_fail) {
                        PLOG_VERBOSE << "Incoming SRTP packet is a replay.";
                    }else if (err == srtp_err_status_auth_fail) {
                        PLOG_VERBOSE << "Incoming SRTP packet failed authentication check.";
                    }else {
                        PLOG_VERBOSE << "SRTCP unprotect error, status: " << err;
                    }
                    return;
                }
                PLOG_VERBOSE << "Unprotected SRTP packet, size: " << unprotected_data_size;
                // TODO: To parse rtp and get ssrc
                // unsigned int ssrc = 0;
                // shrink size
                in_packet->resize(unprotected_data_size);
                // auto rtp_packet = RtpPacket::Create(std::move(in_packet), RtpPacket::Type::RTP, ssrc);
                // if (rtp_packet_recv_callback_) {
                //     rtp_packet_recv_callback_(rtp_packet);
                // }
            }else {
                PLOG_WARNING << "Incoming packet is neither a RTP packet nor a RTCP packet, ignoring.";
            }
        }else {
            PLOG_WARNING << "Incoming packet is neither a RTP/RTCP packet nor a DTLS packet, ignoring.";
        }
    });
}
    
} // namespace naivertc
