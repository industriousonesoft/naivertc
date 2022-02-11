#include "rtc/pc/peer_connection.hpp"
#include "rtc/transports/dtls_srtp_transport.hpp"

#include <plog/Log.h>

namespace naivertc {

void PeerConnection::InitDtlsTransport() {
    RTC_RUN_ON(signaling_task_queue_);
    if (dtls_transport_) {
        return;
    }
    assert(ice_transport_ && "No underlying ICE transport for DTLS transport");

    PLOG_VERBOSE << "Init DTLS transport";
    
    // NOTE: The thread might be blocked here until the certificate has been created.
    auto dtls_config = DtlsTransport::Configuration();
    dtls_config.certificate = certificate_.get();
    dtls_config.mtu = rtc_config_.mtu;

    bool has_media = local_sdp_->HasAudio() || local_sdp_->HasVideo();
    auto lower = ice_transport_.get();
 
    network_task_queue_->Async([this, has_media, lower, config=std::move(dtls_config)](){
        bool is_dtls_client = ice_transport_->role() == sdp::Role::ACTIVE;
        // DTLS-SRTP
        if (has_media) {
            auto dtls_srtp_transport = std::make_unique<DtlsSrtpTransport>(std::move(config), is_dtls_client, lower);
            dtls_srtp_transport->OnReceivedRtpPacket(std::bind(&PeerConnection::OnRtpPacketReceived, this, std::placeholders::_1, std::placeholders::_2));
            dtls_transport_ = std::move(dtls_srtp_transport);
        // DTLS only
        } else {
            dtls_transport_ = std::make_unique<DtlsTransport>(std::move(config), is_dtls_client, lower);
        }

        assert(dtls_transport_ && "Failed to init DTLS transport");

        dtls_transport_->OnStateChanged(std::bind(&PeerConnection::OnDtlsTransportStateChanged, this, std::placeholders::_1));
        dtls_transport_->OnVerify(std::bind(&PeerConnection::OnDtlsVerify, this, std::placeholders::_1));
        
        dtls_transport_->Start();
    });
}

void PeerConnection::OnDtlsTransportStateChanged(DtlsTransport::State transport_state) {
    RTC_RUN_ON(network_task_queue_);
    signaling_task_queue_->Async([this, transport_state](){
        switch (transport_state)
        {
        case DtlsSrtpTransport::State::CONNECTED: {
            PLOG_DEBUG << "DTLS transport connected";
            // DataChannel enabled
            if (this->remote_sdp_ && this->remote_sdp_->HasApplication()) {
                this->InitSctpTransport();
            } else {
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
    RTC_RUN_ON(network_task_queue_);
    return signaling_task_queue_->Sync<bool>([this, remote_fingerprint=std::move(fingerprint)](){
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
    RTC_RUN_ON(network_task_queue_);
    worker_task_queue_->Async([this, in_packet=std::move(in_packet), is_rtcp]() mutable {
        broadcaster_.DeliverRtpPacket(std::move(in_packet), is_rtcp);
    });
}

void PeerConnection::OpenMediaTracks() {
    RTC_RUN_ON(signaling_task_queue_);
    for (auto& kv : media_tracks_) {
        if (auto media_track = kv.second.lock()) {
            if(!media_track->is_opened()) {
                std::static_pointer_cast<MediaChannel>(media_track)->Open();
            }
        }
    }
}

void PeerConnection::CloseMediaTracks() {
    RTC_RUN_ON(signaling_task_queue_);
    for (auto& kv : media_tracks_) {
        if (auto media_track = kv.second.lock()) {
            std::static_pointer_cast<MediaChannel>(media_track)->Close();
        }
    }
    media_tracks_.clear();
}

} // namespace naivertc
