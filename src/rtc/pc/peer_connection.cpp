#include "rtc/pc/peer_connection.hpp"
#include "common/logger.hpp"

#include <plog/Log.h>

#include <variant>
#include <string>

namespace naivertc {

PeerConnection::PeerConnection(const RtcConfiguration config) 
    : rtc_config_(std::move(config)),
    certificate_(Certificate::MakeCertificate(rtc_config_.certificate_type)) {

    if (rtc_config_.port_range_end > 0 && rtc_config_.port_range_end < rtc_config_.port_range_begin) {
        throw std::invalid_argument("Invaild port range.");
    }

    if (auto mtu = rtc_config_.mtu) {
        // Min MTU for IPv4
        if (mtu < 576) {
            throw std::invalid_argument("Invalid MTU value: " + std::to_string(*mtu));
        }else if (mtu > 1500) {
            PLOG_WARNING << "MTU set to: " << *mtu;
        }else {
            PLOG_VERBOSE << "MTU set to: " << *mtu;
        }
    }

    signal_task_queue_ = std::make_unique<TaskQueue>("SignalTaskQueue");
    network_task_queue_ = std::make_shared<TaskQueue>("NetworkTaskQueue");
    // work_task_queue_ = std::make_unique<TaskQueue>("WorkTaskQueue");

    InitIceTransport();
}

PeerConnection::~PeerConnection() {
    CloseTransports();

    signal_task_queue_.reset();
    network_task_queue_.reset();
    work_task_queue_.reset();
}

void PeerConnection::Close() {

    signal_task_queue_->Async([this](){
        PLOG_VERBOSE << "Closing PeerConnection";

        this->negotiation_needed_ = false;

        // TODO: Close data channels asynchronously

        CloseTransports();
    });

}

void PeerConnection::ResetCallbacks() {
    connection_state_callback_ = nullptr;
    gathering_state_callback_ = nullptr;
    candidate_callback_ = nullptr;
    signaling_state_callback_ = nullptr;
}

void PeerConnection::CloseTransports() {
    if (!UpdateConnectionState(ConnectionState::CLOSED)) {
        // Closed already
        return;
    }

    ResetCallbacks();

    if (sctp_transport_) {
        sctp_transport_->Stop();
        sctp_transport_.reset();  
    }
    if (dtls_transport_) {
        dtls_transport_->Stop();
        dtls_transport_.reset();
    }
    if (ice_transport_) {
        ice_transport_->Stop();
        ice_transport_.reset();
    }

}

bool PeerConnection::UpdateConnectionState(ConnectionState state) {
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
    if (signaling_state_ == state) {
        return false;
    }
    signaling_state_ = state;
    if (signaling_state_callback_) {
        signaling_state_callback_(signaling_state_);
    }
    return true;
}

std::string PeerConnection::signaling_state_to_string(SignalingState state) {
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

void PeerConnection::OnIceCandidate(CandidateCallback callback) {
    signal_task_queue_->Async([this, callback](){
        this->candidate_callback_ = callback;
    });
}

void PeerConnection::OnSignalingStateChanged(SignalingStateCallback callback) {
    signal_task_queue_->Async([this, callback](){
        this->signaling_state_callback_ = callback;
    });
}
    
}