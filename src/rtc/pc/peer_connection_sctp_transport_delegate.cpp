#include "rtc/pc/peer_connection.hpp"
#include "base/internals.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

namespace naivertc {
// Init SctpTransport
void PeerConnection::InitSctpTransport() {
    try {
        if (sctp_transport_) {
            return;
        }
        PLOG_VERBOSE << "Starting SCTP transport";

        auto lower = dtls_transport_;
        if (!lower) {
            throw std::logic_error("No underlying DTLS transport for SCTP transport");
        }
        
        if (!remote_sdp_ || !remote_sdp_->application()) {
            throw std::logic_error("Failed to start to create SCTP transport without application sdp.");
        }

        uint16_t sctp_port = remote_sdp_->application()->sctp_port().value_or(DEFAULT_SCTP_PORT);

        // This is the last chance to ensure the stream numbers are coherent with the role
        ShiftDataChannelIfNeccessary();

        // Create SCTP tansport
        SctpTransport::Config sctp_config;
        sctp_config.port = sctp_port;
        sctp_config.mtu = rtc_config_.mtu.value_or(DEFAULT_MTU_SIZE);
        sctp_config.max_message_size = rtc_config_.max_message_size.value_or(DEFAULT_LOCAL_MAX_MESSAGE_SIZE);

        sctp_transport_ = std::make_shared<SctpTransport>(lower, std::move(sctp_config));

        if (!sctp_transport_) {
            throw std::logic_error("Failed to init SCTP transport");
        }

        sctp_transport_->OnStateChanged(std::bind(&PeerConnection::OnSctpTransportStateChanged, this, std::placeholders::_1));
        sctp_transport_->OnSignalBufferedAmountChanged(std::bind(&PeerConnection::OnBufferedAmountChanged, this, std::placeholders::_1, std::placeholders::_2));
        sctp_transport_->OnPacketReceived(std::bind(&PeerConnection::OnSctpPacketReceived, this, std::placeholders::_1));

        sctp_transport_->Start();

    }catch(const std::exception& exp) {
        PLOG_ERROR << exp.what();
		UpdateConnectionState(ConnectionState::FAILED);
		throw std::runtime_error("ICE transport initialization failed");
    }
}

// SctpTransport delegate
void PeerConnection::OnSctpTransportStateChanged(Transport::State transport_state) {
    handle_queue_.Post([this, transport_state](){
        switch(transport_state) {
        case SctpTransport::State::CONNECTED:
            this->UpdateConnectionState(ConnectionState::CONNECTED);
            // TODO: open data channel
            break;
        case SctpTransport::State::FAILED:
            PLOG_WARNING << "SCTP transport failed";
            this->UpdateConnectionState(ConnectionState::FAILED);
            // TODO: open close channel
            break;
        case SctpTransport::State::DISCONNECTED:
            this->UpdateConnectionState(ConnectionState::DISCONNECTED);
            // TODO: open close channel
            break;
        default:
            break;
        }
    });
}

void PeerConnection::OnBufferedAmountChanged(StreamId stream_id, size_t amount) {

}

void PeerConnection::OnSctpPacketReceived(std::shared_ptr<Packet> in_packet) {

}
    
} // namespace naivertc
