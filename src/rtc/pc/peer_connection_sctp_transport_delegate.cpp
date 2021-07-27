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

        sctp_transport_ = std::make_shared<SctpTransport>(std::move(sctp_config), lower, network_task_queue_);

        if (!sctp_transport_) {
            throw std::logic_error("Failed to init SCTP transport");
        }

        sctp_transport_->OnStateChanged(std::bind(&PeerConnection::OnSctpTransportStateChanged, this, std::placeholders::_1));
        sctp_transport_->OnBufferedAmountChanged(std::bind(&PeerConnection::OnBufferedAmountChanged, this, std::placeholders::_1, std::placeholders::_2));
        sctp_transport_->OnPacketReceived(std::bind(&PeerConnection::OnSctpMessageReceived, this, std::placeholders::_1));

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

void PeerConnection::OnSctpMessageReceived(std::shared_ptr<Packet> in_packet) {
    signal_task_queue_->Async([this, in_packet=std::move(in_packet)](){
        if (!in_packet || !utils::instance_of<SctpMessage>(in_packet.get())) {
            return;
        }
        auto message = std::dynamic_pointer_cast<SctpMessage>(in_packet);
        auto stream_id = message->stream_id();
        auto data_channel = FindDataChannel(stream_id);
        if (!data_channel) {
            if (!ice_transport_ || !sctp_transport_) {
                return;
            }
            // Create a remote data channel
            if (DataChannel::IsOpenMessage(message)) {
                // FRC 8832: The peer that initiates opening a data channel selects a stream identifier for 
                // which the corresponding incoming and outgoing streams are unused. If the side is acting as the DTLS client,
                // it MUST choose an even stream identifier, if the side is acting as the DTLS server, it MUST choose an odd one.
                // See https://tools.ietf.org/html/rfc8832#section-6
                StreamId remote_parity = ice_transport_->role() == sdp::Role::ACTIVE ? 1 : 0;
                if (stream_id % 2 == remote_parity) {
                    auto remote_data_channel = std::make_shared<DataChannel>(stream_id);
                    remote_data_channel->AttachTo(sctp_transport_);
                    remote_data_channel->OnOpened(std::bind(&PeerConnection::OnRemoteDataChannelOpened, this, std::placeholders::_1));
                    data_channels_.emplace(std::make_pair(stream_id, remote_data_channel));
                    remote_data_channel->OnIncomingMessage(message);
                }else {
                    PLOG_WARNING << "Try to close a received remote data channel with invalid stream id: " << stream_id;
                    sctp_transport_->ShutdownStream(stream_id);
                    return;
                }
            }
        }else {
            data_channel->OnIncomingMessage(message);
        }
    });
}

void PeerConnection::OnRemoteDataChannelOpened(StreamId stream_id) {
    signal_task_queue_->Async([this, stream_id](){
        auto data_channel = FindDataChannel(stream_id);
        if (data_channel) {
            if (this->data_channel_callback_) {
                this->data_channel_callback_(data_channel);
            }else {
                pending_data_channels_.emplace_back(data_channel);
            }
        }
    });
}

void PeerConnection::OpenDataChannels() {
    if (!sctp_transport_) {
        PLOG_WARNING << "Can not open data channel without SCTP transport";
        return;
    }
    for (auto it = data_channels_.begin(); it != data_channels_.end(); ++it) {
        auto data_channel = it->second;
        data_channel->AttachTo(sctp_transport_);
        data_channel->Open();
    }
}

void PeerConnection::CloseDataChannels() {
    for (auto it = data_channels_.begin(); it != data_channels_.end(); ++it) {
        auto data_channel = it->second;
        data_channel->Close();
    }
    data_channels_.clear();
}

void PeerConnection::RemoteCloseDataChannels() {
    for (auto it = data_channels_.begin(); it != data_channels_.end(); ++it) {
        auto data_channel = it->second;
        data_channel->RemoteClose();
    }
    data_channels_.clear();
}

std::shared_ptr<DataChannel> PeerConnection::FindDataChannel(StreamId stream_id) {
    if (auto it = data_channels_.find(stream_id); it != data_channels_.end()) {
        return it->second;
    }
    return nullptr;
}
    
} // namespace naivertc
