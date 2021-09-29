#include "rtc/pc/peer_connection.hpp"
#include "rtc/transports/dtls_srtp_transport.hpp"

#include <plog/Log.h>

namespace naivertc {

void PeerConnection::InitDtlsTransport() {
    try {
        if (dtls_transport_) {
            return;
        }

        PLOG_VERBOSE << "Init DTLS transport";

        auto lower = ice_transport_;
        if (!lower) {
            throw std::logic_error("No underlying ICE transport for DTLS transport");
        }

        auto certificate = certificate_.get();

        auto dtls_init_config = DtlsTransport::Configuration();
        dtls_init_config.certificate = std::move(certificate);
        dtls_init_config.mtu = rtc_config_.mtu;

        // DTLS-SRTP
        if (auto local_sdp = local_sdp_; local_sdp && (local_sdp->HasAudio() || local_sdp->HasVideo())) {
            auto dtls_srtp_transport = std::make_shared<DtlsSrtpTransport>(std::move(dtls_init_config), lower, network_task_queue_);
            dtls_srtp_transport->OnReceivedRtpPacket(std::bind(&PeerConnection::OnRtpPacketReceived, this, std::placeholders::_1, std::placeholders::_2));
            dtls_transport_ = dtls_srtp_transport;
        // DTLS only
        }else {
            dtls_transport_ = std::make_shared<DtlsTransport>(std::move(dtls_init_config), lower, network_task_queue_);
        }

        if (!dtls_transport_) {
            throw std::logic_error("Failed to init DTLS transport");
        }

        dtls_transport_->OnStateChanged(std::bind(&PeerConnection::OnDtlsTransportStateChanged, this, std::placeholders::_1));
        dtls_transport_->OnVerify(std::bind(&PeerConnection::OnDtlsVerify, this, std::placeholders::_1));
        
        dtls_transport_->Start();
        
    }catch (const std::exception& exp) {
        PLOG_ERROR << "Failed to init dtls transport: " << exp.what();
        UpdateConnectionState(ConnectionState::FAILED);
        throw std::runtime_error("DTLS transport initialization failed");
    }
}

void PeerConnection::OnDtlsTransportStateChanged(DtlsTransport::State transport_state) {
    signal_task_queue_->Async([this, transport_state](){
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
            this->OpenMediaTracks();
            break;
        }
        case DtlsSrtpTransport::State::FAILED: {
            this->UpdateConnectionState(ConnectionState::FAILED);
            this->CloseMediaTracks();
            PLOG_DEBUG << "DTLS transport failed";
            break;
        }
        case DtlsSrtpTransport::State::DISCONNECTED: {
            this->UpdateConnectionState(ConnectionState::DISCONNECTED);
            this->CloseMediaTracks();
            PLOG_DEBUG << "DTLS transport dicconnected";
            break;
        }
        default:
            break;
        }
    });
}

bool PeerConnection::OnDtlsVerify(std::string_view fingerprint) {
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
    worker_task_queue_->Async([this, in_packet=std::move(in_packet), is_rtcp]() mutable {
        rtp_demuxer_.OnRtpPacket(in_packet, is_rtcp);
    });
}

void PeerConnection::OpenMediaTracks() {
    assert(signal_task_queue_->is_in_current_queue());
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
    assert(signal_task_queue_->is_in_current_queue());
    for (auto& kv : media_tracks_) {
        if (auto media_track = kv.second.lock()) {
            media_track->Close();
        }
    }
    media_tracks_.clear();
}

} // namespace naivertc
