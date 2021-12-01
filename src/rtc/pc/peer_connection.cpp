#include "rtc/pc/peer_connection.hpp"
#include "common/logger.hpp"

#include <plog/Log.h>

#include <variant>
#include <string>

namespace naivertc {

PeerConnection::PeerConnection(const RtcConfiguration& config) 
    : rtc_config_(config),
      role_(sdp::Role::ACT_PASS),
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

    signal_task_queue_ = std::make_unique<TaskQueue>("PeerConnection.signal.task.queue");
    network_task_queue_ = std::make_unique<TaskQueue>("PeerConnection.network.task.queue");
    worker_task_queue_ = std::make_unique<TaskQueue>("PeerConnection.worker.task.queue");

    signal_task_queue_->Async([this](){
        InitIceTransport();
    });
}

PeerConnection::~PeerConnection() {
    Close();
    // Those task queues will be blocked until
    // all the tasks in the queue have been done. 
    signal_task_queue_.reset();
    network_task_queue_.reset();
    worker_task_queue_.reset();
}

void PeerConnection::Close() {
    worker_task_queue_->Sync([this](){
        this->CloseDataChannels();
        this->CloseMediaTracks();
    });
    network_task_queue_->Sync([this](){
        this->CloseTransports();
    });
    signal_task_queue_->Sync([this](){
        PLOG_VERBOSE << "Closing PeerConnection";
        this->UpdateConnectionState(ConnectionState::CLOSED);
        this->negotiation_needed_ = false;
        this->data_channel_needed_ = false;
        this->ResetCallbacks();
    });
}

// state && candidate callback
void PeerConnection::OnConnectionStateChanged(ConnectionStateCallback callback) {
    signal_task_queue_->Async([this, callback](){
        this->connection_state_callback_ = callback;
    });
}

void PeerConnection::OnIceGatheringStateChanged(GatheringStateCallback callback) {
    signal_task_queue_->Async([this, callback](){
        this->gathering_state_callback_ = callback;
    });
}

void PeerConnection::OnIceCandidateGathered(CandidateCallback callback) {
    signal_task_queue_->Async([this, callback](){
        this->candidate_callback_ = callback;
    });
}

void PeerConnection::OnSignalingStateChanged(SignalingStateCallback callback) {
    signal_task_queue_->Async([this, callback](){
        this->signaling_state_callback_ = callback;
    });
}

void PeerConnection::OnRemoteDataChannelReceived(DataChannelCallback callback) {
    worker_task_queue_->Async([this, callback=std::move(callback)](){
        this->data_channel_callback_ = std::move(callback);
        // Flush pending data channels
        this->FlushPendingDataChannels();
    });
}

void PeerConnection::OnRemoteMediaTrackReceived(MediaTrackCallback callback) {
    worker_task_queue_->Async([this, callback=std::move(callback)](){
        this->media_track_callback_ = std::move(callback);
        // Flush pending media tracks
        this->FlushPendingMediaTracks();
    });
}

// Private methods
void PeerConnection::FlushPendingDataChannels() {
    RTC_RUN_ON(worker_task_queue_);
    if (this->data_channel_callback_ && this->pending_data_channels_.size() > 0) {
        for (auto dc : this->pending_data_channels_) {
            this->data_channel_callback_(std::move(dc));
        }
        this->pending_data_channels_.clear();
    }
}

void PeerConnection::FlushPendingMediaTracks() {
    RTC_RUN_ON(worker_task_queue_);
    if (this->media_track_callback_ && this->pending_media_tracks_.size() > 0) {
        for (auto dc : this->pending_media_tracks_) {
            this->media_track_callback_(std::move(dc));
        }
        this->pending_media_tracks_.clear();
    }
}

std::shared_ptr<DataChannel> PeerConnection::FindDataChannel(uint16_t stream_id) const {
    RTC_RUN_ON(worker_task_queue_);
    if (auto it = data_channels_.find(stream_id); it != data_channels_.end()) {
        return it->second.lock();
    }
    return nullptr;
}

std::shared_ptr<MediaTrack> PeerConnection::FindMediaTrack(std::string mid) const {
    RTC_RUN_ON(worker_task_queue_);
    if (auto it = this->media_tracks_.find(mid); it != this->media_tracks_.end()) {
        return it->second.lock();
    }
    return nullptr;
}

void PeerConnection::ShiftDataChannelIfNeccessary(sdp::Role role) {
    RTC_RUN_ON(worker_task_queue_);
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
void PeerConnection::OnMediaTrackNegotiated(const sdp::Media& remote_sdp) {
    RTC_RUN_ON(signal_task_queue_);
    auto mid = remote_sdp.mid();
    auto local_sdp_it = media_sdps_.find(mid);
    if (local_sdp_it != media_sdps_.end()) {
        // Send SSRCs
        local_sdp_it->second.ForEachSsrc([this, &mid](const sdp::Media::SsrcEntry& ssrc_entry){
            worker_task_queue_->Async([this, mid, ssrc=ssrc_entry.ssrc](){
                if (auto media_track = FindMediaTrack(mid)) {
                    rtp_demuxer_.AddSink(ssrc, media_track);
                }
            });
        });
    }
    
    // Receive SSRCs
    remote_sdp.ForEachSsrc([this, &mid](const sdp::Media::SsrcEntry& ssrc_entry){
        worker_task_queue_->Async([this, mid, ssrc=ssrc_entry.ssrc](){
            if (auto media_track = FindMediaTrack(mid)) {
                rtp_demuxer_.AddSink(ssrc, media_track);
            }
        });
    });
}

bool PeerConnection::UpdateConnectionState(ConnectionState state) {
    RTC_RUN_ON(signal_task_queue_);
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
    RTC_RUN_ON(signal_task_queue_);
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
    RTC_RUN_ON(signal_task_queue_);
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
    RTC_RUN_ON(signal_task_queue_);
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

std::string PeerConnection::ToString(SignalingState state) {
    switch (state) {
	case SignalingState::STABLE:
		return "stable";
	case SignalingState::HAVE_LOCAL_OFFER:
		return "have-local-offer";
	case SignalingState::HAVE_REMOTE_OFFER:
		return "have-remote-offer";
	case SignalingState::HAVE_LOCAL_PRANSWER:
		return "have-local-pranswer";
	case SignalingState::HAVE_REMOTE_PRANSWER:
		return "have-remote-pranswer";
	default:
		return "unknown";
	}
}

// ostream operator << override
std::ostream& operator<<(std::ostream& out, PeerConnection::ConnectionState state) {
    using State = PeerConnection::ConnectionState;
    switch(state) {
    case State::NEW:
        out << "new";
        break;
    case State::CONNECTING:
        out << "connecting";
        break;
    case State::CONNECTED:
        out << "connected";
        break;
    case State::DISCONNECTED:
        out << "disconnected";
        break;
    case State::FAILED:
        out << "failed";
        break;
    case State::CLOSED:
        out << "closed";
        break;
    default:
        break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, PeerConnection::GatheringState state) {
    using State = PeerConnection::GatheringState;
    switch(state) {
    case State::NEW:
        out << "new";
        break;
    case State::GATHERING:
        out << "gathering";
        break;
    case State::COMPLETED:
        out << "completed";
        break;
    default:
        break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, PeerConnection::SignalingState state) {
    using State = PeerConnection::SignalingState;
    switch(state) {
    case State::STABLE:
        out << "stable";
        break;
    case State::HAVE_LOCAL_OFFER:
        out << "has local offer";
        break;
    case State::HAVE_LOCAL_PRANSWER:
        out << "have local pranswer";
        break;
    case State::HAVE_REMOTE_OFFER:
        out << "has remote offer";
        break;
    case State::HAVE_REMOTE_PRANSWER:
        out << "have remote pranswer";
        break;
    default:
        break;
    }
    return out;
}
    
} // namespace naivertc