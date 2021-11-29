#include "rtc/pc/peer_connection.hpp"
#include "rtc/transports/dtls_srtp_transport.hpp"

#include <plog/Log.h>

namespace naivertc {

DtlsTransport::Configuration PeerConnection::CreateDtlsConfig() const {
    assert(signal_task_queue_->IsCurrent());
    // NOTE: The thread might be blocked here until the certificate has been created.
    auto certificate = certificate_.get();

    auto dtls_config = DtlsTransport::Configuration();
    dtls_config.certificate = std::move(certificate);
    dtls_config.mtu = rtc_config_.mtu;
    return dtls_config;
}

void PeerConnection::InitDtlsTransport(DtlsTransport::Configuration config) {
    assert(network_task_queue_->IsCurrent());
    if (dtls_transport_) {
        return;
    }

    PLOG_VERBOSE << "Init DTLS transport";

    assert(ice_transport_ && "No underlying ICE transport for DTLS transport");
 
    // DTLS-SRTP
    if (auto local_sdp = local_sdp_; local_sdp && (local_sdp->HasAudio() || local_sdp->HasVideo())) {
        auto dtls_srtp_transport = std::make_shared<DtlsSrtpTransport>(std::move(config), ice_transport_);
        dtls_srtp_transport->OnReceivedRtpPacket(std::bind(&PeerConnection::OnRtpPacketReceived, this, std::placeholders::_1, std::placeholders::_2));
        dtls_transport_ = dtls_srtp_transport;
    // DTLS only
    } else {
        dtls_transport_ = std::make_shared<DtlsTransport>(std::move(config), ice_transport_);
    }

    assert(dtls_transport_ && "Failed to init DTLS transport");

    dtls_transport_->OnStateChanged(std::bind(&PeerConnection::OnDtlsTransportStateChanged, this, std::placeholders::_1));
    dtls_transport_->OnVerify(std::bind(&PeerConnection::OnDtlsVerify, this, std::placeholders::_1));
    
    dtls_transport_->Start();
}

void PeerConnection::OnDtlsTransportStateChanged(DtlsTransport::State transport_state) {
    assert(network_task_queue_->IsCurrent());
    signal_task_queue_->Async([this, transport_state](){
        switch (transport_state)
        {
        case DtlsSrtpTransport::State::CONNECTED: {
            PLOG_DEBUG << "DTLS transport connected";
            // DataChannel enabled
            if (this->remote_sdp_ && this->remote_sdp_->HasApplication()) {
                auto sctp_config = CreateSctpConfig();
                this->network_task_queue_->Async([this, config=std::move(sctp_config)](){
                    this->InitSctpTransport(std::move(config));
                });
            } else {
                this->UpdateConnectionState(ConnectionState::CONNECTED);
            }
            break;
        }
        case DtlsSrtpTransport::State::FAILED: {
            this->UpdateConnectionState(ConnectionState::FAILED);
            PLOG_DEBUG << "DTLS transport failed";
            break;
        }
        case DtlsSrtpTransport::State::DISCONNECTED: {
            this->UpdateConnectionState(ConnectionState::DISCONNECTED);
            PLOG_DEBUG << "DTLS transport dicconnected";
            break;
        }
        default:
            break;
        }
    });
    worker_task_queue_->Async([this, transport_state](){
        if (transport_state == DtlsSrtpTransport::State::CONNECTED) {
            this->OpenMediaTracks();
        } else if (transport_state == DtlsSrtpTransport::State::FAILED ||
                   transport_state == DtlsSrtpTransport::State::DISCONNECTED) {
            this->CloseMediaTracks();
        }
    });
}

bool PeerConnection::OnDtlsVerify(std::string_view fingerprint) {
    assert(network_task_queue_->IsCurrent());
    return signal_task_queue_->Sync<bool>([this, remote_fingerprint=std::move(fingerprint)](){
        // We expect the remote fingerprint received by singaling channel is equal to 
        // the remote fingerprint received by DTLS channel.
        auto expected_remote_fingerprint = this->remote_sdp_.has_value() ? this->remote_sdp_->fingerprint() : std::nullopt;
        if (expected_remote_fingerprint.has_value() && expected_remote_fingerprint.value() == remote_fingerprint) {
            PLOG_VERBOSE << "Valid fingerprint : " << remote_fingerprint << " from remote peer.";
            return true;
        }
        PLOG_ERROR << "Invalid fingerprint : " << remote_fingerprint << ", expected : "
	               << expected_remote_fingerprint.value_or("[none]") << ".";
        return false;
    }); 
}

void PeerConnection::OnRtpPacketReceived(CopyOnWriteBuffer in_packet, bool is_rtcp) {
    assert(network_task_queue_->IsCurrent());
    worker_task_queue_->Async([this, in_packet=std::move(in_packet), is_rtcp]() mutable {
        rtp_demuxer_.OnRtpPacket(in_packet, is_rtcp);
    });
}

void PeerConnection::OpenMediaTracks() {
    assert(worker_task_queue_->IsCurrent());
    auto srtp_transport = std::dynamic_pointer_cast<DtlsSrtpTransport>(dtls_transport_);
    for (auto& kv : media_tracks_) {
        if (auto media_track = kv.second.lock()) {
            if (!media_track->is_opened()) {
                media_track->Open(srtp_transport);
            }
        }
    }
}

void PeerConnection::CloseMediaTracks() {
    assert(worker_task_queue_->IsCurrent());
    for (auto& kv : media_tracks_) {
        if (auto media_track = kv.second.lock()) {
            media_track->Close();
        }
    }
    media_tracks_.clear();
}

} // namespace naivertc
