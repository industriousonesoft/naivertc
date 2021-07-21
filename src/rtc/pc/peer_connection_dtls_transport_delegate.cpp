#include "rtc/pc/peer_connection.hpp"
#include "common/utils.hpp"
#include "rtc/transports/dtls_srtp_transport.hpp"

#include <plog/Log.h>

namespace naivertc {

void PeerConnection::InitDtlsTransport() {
    try {
        PLOG_VERBOSE << "Init DTLS transport";

        auto lower = ice_transport_;
        if (!lower) {
            throw std::logic_error("No underlying ICE transport for DTLS transport");
        }

        auto certificate = certificate_.get();

        auto dtls_init_config = DtlsTransport::Config();
        dtls_init_config.certificate = std::move(certificate);
        dtls_init_config.mtu = rtc_config_.mtu;

        std::shared_ptr<DtlsTransport> dtls_transport = nullptr;
        // DTLS-SRTP
        if (auto local_sdp = local_sdp_; local_sdp && (local_sdp->HasAudio() || local_sdp->HasVideo())) {
            auto dtls_srtp_transport = std::make_shared<DtlsSrtpTransport>(lower, std::move(dtls_init_config));
            dtls_srtp_transport->OnReceivedRtpPacket(utils::weak_bind(&PeerConnection::OnRtpPacketReceived, this, std::placeholders::_1));
            dtls_transport = dtls_srtp_transport;
        // DTLS only
        }else {
            dtls_transport = std::make_shared<DtlsTransport>(lower, std::move(dtls_init_config));
        }

        if (!dtls_transport) {
            throw std::logic_error("Failed to init DTLS transport");
        }

        dtls_transport_->OnStateChanged(utils::weak_bind(&PeerConnection::OnDtlsTransportStateChanged, this, std::placeholders::_1));
        dtls_transport_->OnVerify(utils::weak_bind(&PeerConnection::OnDtlsVerify, this, std::placeholders::_1));
        
        dtls_transport->Start();
        
    }catch (const std::exception& exp) {
        PLOG_ERROR << "Failed to init dtls transport: " << exp.what();
        UpdateConnectionState(ConnectionState::FAILED);
        throw std::runtime_error("DTLS transport initialization failed");
    }
}

void PeerConnection::OnDtlsTransportStateChanged(DtlsTransport::State transport_state) {
    handle_queue_.Post([this, transport_state](){
        switch (transport_state)
        {
        case DtlsSrtpTransport::State::CONNECTED: {
            PLOG_DEBUG << "DTLS transport connected";
            // DataChannel enabled
            if (auto remote_sdp = this->remote_sdp_; remote_sdp && remote_sdp->HasApplication()) {
                this->InitSctpTransport();
            }else {
                this->UpdateConnectionState(ConnectionState::CONNECTED);
            }
            // TODO: To open local media tracks
            break;
        }
        case DtlsSrtpTransport::State::FAILED: {
            this->UpdateConnectionState(ConnectionState::FAILED);
            break;
        }
        case DtlsSrtpTransport::State::DISCONNECTED: {
            this->UpdateConnectionState(ConnectionState::DISCONNECTED);
            break;
        }
        default:
            break;
        }
    });
}

bool PeerConnection::OnDtlsVerify(std::string_view fingerprint) {
    return true;
}

void PeerConnection::OnRtpPacketReceived(std::shared_ptr<RtpPacket> in_packet) {
    
}

} // namespace naivertc
