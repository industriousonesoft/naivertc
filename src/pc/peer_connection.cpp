#include "pc/peer_connection.hpp"

#include <plog/Log.h>

#include <variant>
#include <string>

namespace naivertc {

PeerConnection::PeerConnection(Configuration config) 
    : config_(config),
    certificate_(Certificate::MakeCertificate(config_.certificate_type)),
    connection_state_(ConnectionState::CLOSED),
    gathering_state_(GatheringState::NONE),
    negotiation_needed_(false) {

    if (config_.port_range_end_ > 0 && config_.port_range_end_ <  config_.port_range_begin_) {
        throw std::invalid_argument("Invaild port range.");
    }

    if (auto mtu = config_.mtu_) {
        // Min MTU for IPv4
        if (mtu < 576) {
            throw std::invalid_argument("Invalid MTU value: " + std::to_string(*mtu));
        }else if (mtu > 1500) {
            PLOG_WARNING << "MTU set to: " << *mtu;
        }else {
            PLOG_VERBOSE << "MTU set to: " << *mtu;
        }
    }

    InitIceTransport();
}

PeerConnection::~PeerConnection() {
    ice_transport_.reset();
    sctp_transport_.reset();
}

// Private methods
bool PeerConnection::UpdateConnectionState(ConnectionState state) {
    if (connection_state_ == state) {
        return false;
    }
    connection_state_ = state;
    this->connection_state_callback_(connection_state_);
    return true;
}

bool PeerConnection::UpdateGatheringState(GatheringState state) {
    if (gathering_state_ == state) {
        return false;
    }
    gathering_state_ = state;
    this->gathering_state_callback_(gathering_state_);
    return true;
}

bool PeerConnection::UpdateSignalingState(SignalingState state) {
    if (signaling_state_ == state) {
        return false;
    }
    signaling_state_ = state;
    signaling_state_callback_(signaling_state_);
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
    handle_queue_.Post([this, callback](){
        this->connection_state_callback_ = callback;
    });
}

void PeerConnection::OnIceGatheringStateChanged(GatheringStateCallback callback) {
    handle_queue_.Post([this, callback](){
        this->gathering_state_callback_ = callback;
    });
}

void PeerConnection::OnIceCandidate(CandidateCallback callback) {
    handle_queue_.Post([this, callback](){
        this->candidate_callback_ = callback;
    });
}

void PeerConnection::OnSignalingStateChanged(SignalingStateCallback callback) {
    handle_queue_.Post([this, callback](){
        this->signaling_state_callback_ = callback;
    });
}
    
}