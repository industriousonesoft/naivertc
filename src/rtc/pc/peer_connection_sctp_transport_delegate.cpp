#include "rtc/pc/peer_connection.hpp"
#include "rtc/base/internals.hpp"
#include "rtc/transports/sctp_transport_internals.hpp"

#include <plog/Log.h>

namespace naivertc {

// Init SctpTransport
void PeerConnection::InitSctpTransport() {
    RTC_RUN_ON(signaling_task_queue_);
    if (sctp_transport_) {
        return;
    }
    PLOG_VERBOSE << "Starting SCTP transport";
    assert(dtls_transport_ && "No underlying DTLS transport for SCTP transport");
    assert(remote_sdp_->HasApplication());

    uint16_t sctp_port = rtc_config_.local_sctp_port.value_or(kDefaultSctpPort);
    // Create SCTP tansport
    SctpTransport::Configuration sctp_config;
    sctp_config.port = sctp_port;
    sctp_config.mtu = rtc_config_.mtu.value_or(kDefaultMtuSize);
    sctp_config.max_message_size = rtc_config_.sctp_max_message_size.value_or(kDefaultSctpMaxMessageSize);

    network_task_queue_->Post([this, config=std::move(sctp_config)](){
        sctp_transport_ = std::make_unique<SctpTransport>(std::move(config), dtls_transport_.get());
        assert(sctp_transport_ && "Failed to init SCTP transport");
        sctp_transport_->OnStateChanged(std::bind(&PeerConnection::OnSctpTransportStateChanged, this, std::placeholders::_1));
        sctp_transport_->OnSctpMessageReceived(std::bind(&PeerConnection::OnSctpMessageReceived, this, std::placeholders::_1));

        sctp_transport_->Start();
    });
    
}

// SctpTransport delegate
void PeerConnection::OnSctpTransportStateChanged(Transport::State state) {
    RTC_RUN_ON(network_task_queue_);
    signaling_task_queue_->Post([this, state](){
        switch(state) {
        case SctpTransport::State::CONNECTED:
            PLOG_DEBUG << "SCTP transport connected";
            UpdateConnectionState(ConnectionState::CONNECTED);
            OpenDataChannels();
            break;
        case SctpTransport::State::FAILED:
            PLOG_WARNING << "SCTP transport failed";
            UpdateConnectionState(ConnectionState::FAILED);
            RemoteCloseDataChannels();
            break;
        case SctpTransport::State::DISCONNECTED:
            UpdateConnectionState(ConnectionState::DISCONNECTED);
            RemoteCloseDataChannels();
            PLOG_DEBUG << "SCTP transport disconnected";
            break;
        default:
            break;
        }
    });
}

void PeerConnection::OnSctpMessageReceived(SctpMessage message) {
    RTC_RUN_ON(network_task_queue_);
    signaling_task_queue_->Post([this, message=std::move(message)](){
        auto stream_id = message.stream_id();
        auto data_channel = FindDataChannel(stream_id);
        if (!data_channel) {
            // Response a remote data channel
            if (message.type() == SctpMessage::Type::CONTROL && DataChannel::IsOpenMessage(message.payload())) {
                // FRC 8832: The peer that initiates opening a data channel selects a stream identifier for 
                // which the corresponding incoming and outgoing streams are unused. If the side is acting as the DTLS client,
                // it MUST choose an even stream identifier, if the side is acting as the DTLS server, it MUST choose an odd one.
                // See https://tools.ietf.org/html/rfc8832#section-6
                bool is_remote_a_dtls_server = network_task_queue_->Invoke<bool>([this](){
                    return ice_transport_->role() == sdp::Role::ACTIVE ? true : false;
                });
                uint16_t remote_parity = is_remote_a_dtls_server ? 1 : 0;
                if (stream_id % 2 == remote_parity) {
                    // The remote data channel will negotiate later by processing incomming message, 
                    // so it's unnegotiated.
                    data_channel = DataChannel::RemoteDataChannel(stream_id, /*negotiated=*/false, shared_from_this());
                    // We own the data channel temporarily
                    pending_data_channels_.push_back(data_channel);
                    data_channels_.emplace(stream_id, data_channel);
                    data_channel->OnOpened([this, data_channel](){
                        // Add incoming data channel after it's opened.
                        this->OnIncomingDataChannel(data_channel);
                    });
                    data_channel->OnIncomingMessage(std::move(message));
                } else {
                    PLOG_WARNING << "Failed to response the data channel created by remote peer, since it's stream id [" << stream_id
                                    << "] is not corresponding to the remote role.";
                    network_task_queue_->Post([this, stream_id](){
                        sctp_transport_->CloseStream(stream_id);
                    });
                    return;
                }
            } else {
                PLOG_WARNING << "No data channel found to handle non-opening incoming message with stream id: " << stream_id;
                network_task_queue_->Post([this, stream_id](){
                    sctp_transport_->CloseStream(stream_id);
                });
                return;
            }
        } else {
            data_channel->OnIncomingMessage(std::move(message));
        }
    });
}

void PeerConnection::OnSctpReadyToSend() {
    RTC_RUN_ON(network_task_queue_);
    signaling_task_queue_->Post([this](){
        for (auto& kv : data_channels_) {
            if (auto dc = kv.second.lock()) {
                dc->OnReadyToSend();
            }
        }
    });
}

// Helper methods
void PeerConnection::OpenDataChannels() {
    RTC_RUN_ON(signaling_task_queue_);
    for (auto& kv : data_channels_) {
        if (auto dc = kv.second.lock()) {
            dc->Open(shared_from_this());
        }
    }
}

void PeerConnection::CloseDataChannels() {
    RTC_RUN_ON(signaling_task_queue_);
    for (auto& kv : data_channels_) {
        if (auto dc = kv.second.lock()) {
            dc->Close();
        }
    }
    data_channels_.clear();
}

void PeerConnection::RemoteCloseDataChannels() {
    RTC_RUN_ON(signaling_task_queue_);
    for (auto& kv : data_channels_) {
        if (auto dc = kv.second.lock()) {
            dc->Close(/*by_remote=*/true);
        }
    }
    data_channels_.clear();
}

void PeerConnection::OnIncomingDataChannel(std::shared_ptr<DataChannel> data_channel) {
    RTC_RUN_ON(signaling_task_queue_);
    if (data_channel_callback_) {
        data_channel_callback_(std::move(data_channel));
    } else {
        pending_data_channels_.push_back(std::move(data_channel));
    }
}

} // namespace naivertc
