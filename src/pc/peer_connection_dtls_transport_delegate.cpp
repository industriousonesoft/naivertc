#include "pc/peer_connection.hpp"
#include "common/utils.hpp"
#include "pc/transports/dtls_srtp_transport.hpp"

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
        dtls_init_config.mtu = config_.mtu;

        // DTLS-SRTP
        if (auto local_sdp = local_session_description_; local_sdp.has_value() && (local_sdp->HasAudio() || local_sdp->HasVieo())) {
            dtls_transport_ = std::make_shared<DtlsSrtpTransport>(lower, std::move(dtls_init_config));
            dtls_transport_->OnPacketReceived(utils::weak_bind(&PeerConnection::OnDtlsPacketReceived, this, std::placeholders::_1));
        // DTLS only
        }else {
            dtls_transport_ = std::make_shared<DtlsTransport>(lower, std::move(dtls_init_config));
            dtls_transport_->OnPacketReceived(nullptr);
        }

        dtls_transport_->SignalStateChanged.connect(this, &PeerConnection::OnDtlsTransportStateChange);
        dtls_transport_->OnVerify(utils::weak_bind(&PeerConnection::OnDtlsVerify, this, std::placeholders::_1));
        

    }catch (const std::exception& exp) {
        PLOG_ERROR << "Failed to init dtls transport: " << exp.what();
        UpdateConnectionState(ConnectionState::FAILED);
        throw std::runtime_error("DTLS transport initialization failed");
    }
}

void PeerConnection::OnDtlsTransportStateChange(DtlsTransport::State transport_state) {
    handle_queue_.Post([this, transport_state](){
        switch (transport_state)
        {
        case DtlsSrtpTransport::State::CONNECTED: {
            // DataChannel enabled
            if (auto remote_sdp = this->remote_session_description_; remote_sdp && remote_sdp->HasApplication()) {
                this->InitSctpTransport();
            }else {
                this->UpdateConnectionState(ConnectionState::CONNECTED);
            }
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

bool PeerConnection::OnDtlsVerify(const std::string& fingerprint) {
    return true;
}

void PeerConnection::OnDtlsPacketReceived(std::shared_ptr<Packet> in_packet) {
    
}

} // namespace naivertc
