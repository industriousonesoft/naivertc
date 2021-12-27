#include "rtc/pc/peer_connection.hpp"
#include "rtc/transports/dtls_srtp_transport.hpp"
#include "common/logger.hpp"

#include <plog/Log.h>

#include <variant>
#include <string>

namespace naivertc {

PeerConnection::PeerConnection(const RtcConfiguration& config) 
    : rtc_config_(config),
      clock_(std::make_unique<RealTimeClock>()),
      certificate_(Certificate::MakeCertificate(rtc_config_.certificate_type)) {

    if (rtc_config_.port_range_end > 0 && rtc_config_.port_range_end < rtc_config_.port_range_begin) {
        throw std::invalid_argument("Invaild port range.");
    }

    if (auto mtu = rtc_config_.mtu) {
        // Min MTU for IPv4
        if (mtu < 576) {
            throw std::invalid_argument("Invalid MTU value: " + std::to_string(*mtu));
        } else if (mtu > 1500) {
            PLOG_WARNING << "MTU set to: " << *mtu;
        } else {
            PLOG_VERBOSE << "MTU set to: " << *mtu;
        }
    }

    signaling_task_queue_ = std::make_unique<TaskQueue>("PeerConnection.signal.task.queue");
    network_task_queue_ = std::make_unique<TaskQueue>("PeerConnection.network.task.queue");
    worker_task_queue_ = std::make_unique<TaskQueue>("PeerConnection.worker.task.queue");

    signaling_task_queue_->Async([this](){
        InitIceTransport();
    });
}

PeerConnection::~PeerConnection() {
    Close();
    // Those task queues will be blocked until
    // all the tasks in the queue have been done. 
    signaling_task_queue_.reset();
    network_task_queue_.reset();
    worker_task_queue_.reset();
}

void PeerConnection::Close() {
    worker_task_queue_->Sync([this](){
       rtp_demuxer_.Clear();
    });
    network_task_queue_->Sync([this](){
        this->CloseTransports();
    });
    signaling_task_queue_->Sync([this](){
        PLOG_VERBOSE << "Closing PeerConnection";
        this->negotiation_needed_ = false;
        this->data_channel_needed_ = false;
        this->ResetCallbacks();
        this->CloseDataChannels();
        this->CloseMediaTracks();
    });
}

// state && candidate callback
void PeerConnection::OnConnectionStateChanged(ConnectionStateCallback callback) {
    signaling_task_queue_->Async([this, callback](){
        this->connection_state_callback_ = callback;
    });
}

void PeerConnection::OnIceGatheringStateChanged(GatheringStateCallback callback) {
    signaling_task_queue_->Async([this, callback](){
        this->gathering_state_callback_ = callback;
    });
}

void PeerConnection::OnIceCandidateGathered(CandidateCallback callback) {
    signaling_task_queue_->Async([this, callback](){
        this->candidate_callback_ = callback;
    });
}

void PeerConnection::OnSignalingStateChanged(SignalingStateCallback callback) {
    signaling_task_queue_->Async([this, callback](){
        this->signaling_state_callback_ = callback;
    });
}

void PeerConnection::OnRemoteDataChannelReceived(DataChannelCallback callback) {
    signaling_task_queue_->Async([this, callback=std::move(callback)](){
        this->data_channel_callback_ = std::move(callback);
        // Flush pending data channels
        this->FlushPendingDataChannels();
    });
}

void PeerConnection::OnRemoteMediaTrackReceived(MediaTrackCallback callback) {
    signaling_task_queue_->Async([this, callback=std::move(callback)](){
        this->media_track_callback_ = std::move(callback);
        // Flush pending media tracks
        this->FlushPendingMediaTracks();
    });
}

// MediaTransport interface
int PeerConnection::SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options) {
    return network_task_queue_->Sync<int>([this, packet=std::move(packet), options=std::move(options)](){
        auto srtp_transport = dynamic_cast<DtlsSrtpTransport*>(dtls_transport_.get());
        if (srtp_transport) {
            return srtp_transport->SendRtpPacket(std::move(packet), std::move(options));
        } else {
            return -1;
        }
    });
}

// DataTransport interface
bool PeerConnection::Send(SctpMessageToSend message) {
    return network_task_queue_->Sync<bool>([this, message=std::move(message)](){
        if (sctp_transport_) {
            return sctp_transport_->Send(std::move(message));
        } else {
            return false;
        }
    });
}

// Private methods
void PeerConnection::FlushPendingDataChannels() {
    RTC_RUN_ON(signaling_task_queue_);
    if (this->data_channel_callback_ && this->pending_data_channels_.size() > 0) {
        for (auto dc : this->pending_data_channels_) {
            this->data_channel_callback_(std::move(dc));
        }
        this->pending_data_channels_.clear();
    }
}

void PeerConnection::FlushPendingMediaTracks() {
    RTC_RUN_ON(signaling_task_queue_);
    if (this->media_track_callback_ && this->pending_media_tracks_.size() > 0) {
        for (auto dc : this->pending_media_tracks_) {
            this->media_track_callback_(std::move(dc));
        }
        this->pending_media_tracks_.clear();
    }
}

std::shared_ptr<DataChannel> PeerConnection::FindDataChannel(uint16_t stream_id) const {
    RTC_RUN_ON(signaling_task_queue_);
    if (auto it = data_channels_.find(stream_id); it != data_channels_.end()) {
        return it->second.lock();
    }
    return nullptr;
}

std::shared_ptr<MediaTrack> PeerConnection::FindMediaTrack(std::string mid) const {
    RTC_RUN_ON(signaling_task_queue_);
    if (auto it = this->media_tracks_.find(mid); it != this->media_tracks_.end()) {
        return it->second.lock();
    }
    return nullptr;
}

void PeerConnection::ShiftDataChannelIfNeccessary(sdp::Role role) {
    RTC_RUN_ON(signaling_task_queue_);
    decltype(data_channels_) new_data_channels;
    for (auto& kv : data_channels_) {
        if (auto dc = kv.second.lock()) {
            dc.get()->HintStreamId(role);
            new_data_channels.emplace(dc->stream_id(), dc);
        }
    }
    std::swap(data_channels_, new_data_channels);
}

// Private methods
bool PeerConnection::UpdateConnectionState(ConnectionState state) {
    RTC_RUN_ON(signaling_task_queue_);
    if (connection_state_ == state) {
        return false;
    }
    connection_state_ = state;
    if (connection_state_callback_) {
        connection_state_callback_(connection_state_);
    }
    return true;
}

bool PeerConnection::UpdateGatheringState(GatheringState state) {
    RTC_RUN_ON(signaling_task_queue_);
    if (gathering_state_ == state) {
        return false;
    }
    gathering_state_ = state;
    if (gathering_state_callback_) {
        gathering_state_callback_(gathering_state_);
    }
    return true;
}

bool PeerConnection::UpdateSignalingState(SignalingState state) {
    RTC_RUN_ON(signaling_task_queue_);
    if (signaling_state_ == state) {
        return false;
    }
    signaling_state_ = state;
    if (signaling_state_callback_) {
        signaling_state_callback_(signaling_state_);
    }
    return true;
}

void PeerConnection::ResetCallbacks() {
    RTC_RUN_ON(signaling_task_queue_);
    connection_state_callback_ = nullptr;
    gathering_state_callback_ = nullptr;
    candidate_callback_ = nullptr;
    signaling_state_callback_ = nullptr;
}

void PeerConnection::CloseTransports() {
    RTC_RUN_ON(network_task_queue_);

    if (sctp_transport_) {
        sctp_transport_->Stop();
    }
    if (dtls_transport_) {
        dtls_transport_->Stop();
    }
    if (ice_transport_) {
        ice_transport_->Stop();
    }

    sctp_transport_.reset();  
    dtls_transport_.reset();
    ice_transport_.reset();

}

std::string PeerConnection::ToString(ConnectionState state) {
    using State = PeerConnection::ConnectionState;
    switch(state) {
    case State::NEW:
        return "new";
    case State::CONNECTING:
        return "connecting";
    case State::CONNECTED:
        return "connected";
    case State::DISCONNECTED:
        return "disconnected";
    case State::FAILED:
        return "failed";
    case State::CLOSED:
        return "closed";
    default:
        RTC_NOTREACHED();
    }
}

std::string PeerConnection::ToString(GatheringState state) {
    using State = PeerConnection::GatheringState;
    switch(state) {
    case State::NEW:
        return "new";
    case State::GATHERING:
        return "gathering";
    case State::COMPLETED:
        return "completed";
    default:
        RTC_NOTREACHED();
    }
}

std::string PeerConnection::ToString(SignalingState state) {
    using State = PeerConnection::SignalingState;
    switch (state) {
	case State::STABLE:
		return "stable";
	case State::HAVE_LOCAL_OFFER:
		return "have-local-offer";
	case State::HAVE_REMOTE_OFFER:
		return "have-remote-offer";
	case State::HAVE_LOCAL_PRANSWER:
		return "have-local-pranswer";
	case State::HAVE_REMOTE_PRANSWER:
		return "have-remote-pranswer";
	default:
		RTC_NOTREACHED();
	}
}

// ostream operator << override
std::ostream& operator<<(std::ostream& out, PeerConnection::ConnectionState state) {
    out << PeerConnection::ToString(state);
    return out;
}

std::ostream& operator<<(std::ostream& out, PeerConnection::GatheringState state) {
    out << PeerConnection::ToString(state);
    return out;
}

std::ostream& operator<<(std::ostream& out, PeerConnection::SignalingState state) {
    out << PeerConnection::ToString(state);
    return out;
}
    
} // namespace naivertc