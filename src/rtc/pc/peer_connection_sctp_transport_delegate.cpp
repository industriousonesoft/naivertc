#include "rtc/pc/peer_connection.hpp"
#include "rtc/base/internals.hpp"

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

        uint16_t sctp_port = remote_sdp_->application()->sctp_port().value_or(kDefaultSctpPort);

        // Create SCTP tansport
        SctpTransport::Configuration sctp_config;
        sctp_config.port = sctp_port;
        sctp_config.mtu = rtc_config_.mtu.value_or(kDefaultMtuSize);
        sctp_config.max_message_size = rtc_config_.max_message_size.value_or(kDefaultLocalMaxMessageSize);

        sctp_transport_ = std::make_shared<SctpTransport>(std::move(sctp_config), lower, network_task_queue_);

        if (!sctp_transport_) {
            throw std::logic_error("Failed to init SCTP transport");
        }

        sctp_transport_->OnStateChanged(std::bind(&PeerConnection::OnSctpTransportStateChanged, this, std::placeholders::_1));
        sctp_transport_->OnBufferedAmountChanged(std::bind(&PeerConnection::OnBufferedAmountChanged, this, std::placeholders::_1, std::placeholders::_2));
        sctp_transport_->OnSctpMessageReceived(std::bind(&PeerConnection::OnSctpMessageReceived, this, std::placeholders::_1));

        sctp_transport_->Start();

    }catch(const std::exception& exp) {
        PLOG_ERROR << exp.what();
		UpdateConnectionState(ConnectionState::FAILED);
		throw std::runtime_error("ICE transport initialization failed");
    }
}

// SctpTransport delegate
void PeerConnection::OnSctpTransportStateChanged(Transport::State transport_state) {
    signal_task_queue_->Async([this, transport_state](){
        switch(transport_state) {
        case SctpTransport::State::CONNECTED:
            PLOG_DEBUG << "SCTP transport connected";
            this->UpdateConnectionState(ConnectionState::CONNECTED);
            OpenDataChannels();
            break;
        case SctpTransport::State::FAILED:
            PLOG_WARNING << "SCTP transport failed";
            this->UpdateConnectionState(ConnectionState::FAILED);
            RemoteCloseDataChannels();
            break;
        case SctpTransport::State::DISCONNECTED:
            this->UpdateConnectionState(ConnectionState::DISCONNECTED);
            RemoteCloseDataChannels();
            PLOG_DEBUG << "SCTP transport disconnected";
            break;
        default:
            break;
        }
    });
}

void PeerConnection::OnBufferedAmountChanged(StreamId stream_id, size_t amount) {
    signal_task_queue_->Async([this, stream_id, amount](){
        if (auto data_channel = FindDataChannel(stream_id)) {
            data_channel->OnBufferedAmount(amount);
        }
    });
}

void PeerConnection::OnSctpMessageReceived(SctpMessage message) {
    // TODO: Using work task queue
    signal_task_queue_->Async([this, message=std::move(message)]() mutable {
        if (message.empty()) {
            PLOG_WARNING << "Received empty sctp message";
            return;
        }
        auto stream_id = message.stream_id();
        auto data_channel = FindDataChannel(stream_id);
        if (!data_channel) {
            // Create a remote data channel
            if (DataChannel::IsOpenMessage(message)) {
                // FRC 8832: The peer that initiates opening a data channel selects a stream identifier for 
                // which the corresponding incoming and outgoing streams are unused. If the side is acting as the DTLS client,
                // it MUST choose an even stream identifier, if the side is acting as the DTLS server, it MUST choose an odd one.
                // See https://tools.ietf.org/html/rfc8832#section-6
                bool is_remote_a_dtls_server= ice_transport_->role() == sdp::Role::ACTIVE ? true : false;
                StreamId remote_parity = is_remote_a_dtls_server ? 1 : 0;
                if (stream_id % 2 == remote_parity) {
                    // The remote data channel will negotiate later by processing incomming message, 
                    // so it's unnegotiated.
                    data_channel = DataChannel::RemoteDataChannel(stream_id, false /* unnegotiated */, sctp_transport_);
                    // We own the data channel temporarily
                    pending_data_channels_.push_back(data_channel);
                    data_channels_.emplace(stream_id, data_channel);
                    data_channel->OnOpened([this, data_channel](){
                        // Add incoming data channel after it's opened.
                        this->OnIncomingDataChannel(data_channel);
                    });
                    data_channel->OnIncomingMessage(std::move(message));
                }else {
                    PLOG_WARNING << "Try to close a received remote data channel with invalid stream id: " << stream_id;
                    sctp_transport_->ShutdownStream(stream_id);
                    return;
                }
            }
        }else {
            data_channel->OnIncomingMessage(std::move(message));
        }
    });
}

void PeerConnection::OpenDataChannels() {
    assert(signal_task_queue_->is_in_current_queue());
    for (auto& kv : data_channels_) {
        if (auto dc = kv.second.lock()) {
            dc->Open(sctp_transport_);
        }
    }
}

void PeerConnection::CloseDataChannels() {
    assert(signal_task_queue_->is_in_current_queue());
    for (auto& kv : data_channels_) {
        if (auto dc = kv.second.lock()) {
            dc->Close();
        }
    }
    data_channels_.clear();
}

void PeerConnection::RemoteCloseDataChannels() {
    assert(signal_task_queue_->is_in_current_queue());
    for (auto& kv : data_channels_) {
        if (auto dc = kv.second.lock()) {
            dc->RemoteClose();
        }
    }
    data_channels_.clear();
}

void PeerConnection::OnIncomingDataChannel(std::shared_ptr<DataChannel> data_channel) {
    signal_task_queue_->Async([this, data_channel=std::move(data_channel)](){
        if (data_channel_callback_) {
            data_channel_callback_(std::move(data_channel));
        }else {
            pending_data_channels_.push_back(std::move(data_channel));
        }
    });
}

} // namespace naivertc
