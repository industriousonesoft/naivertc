#include "pc/peer_connection.hpp"
#include "common/logger.hpp"

#include <plog/Log.h>

#include <variant>
#include <string>

namespace naivertc {

PeerConnection::PeerConnection(const RtcConfiguration& config) 
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

    InitLogger(config.logging_level);

    InitIceTransport();
}

PeerConnection::~PeerConnection() {
    ice_transport_.reset();
    sctp_transport_.reset();
}

// Private methods
void PeerConnection::InitLogger(LoggingLevel level) {
    logging::Level plog_level = logging::Level::NONE;
    switch (level) {
    case LoggingLevel::NONE:
        plog_level = logging::Level::NONE;
        break;
    case LoggingLevel::DEBUG:
        plog_level = logging::Level::DEBUG;
        break;
    case LoggingLevel::WARNING:
        plog_level = logging::Level::WARNING;
        break;
    case LoggingLevel::INFO:
        plog_level = logging::Level::INFO;
        break;
    case LoggingLevel::ERROR:
        plog_level = logging::Level::ERROR;
        break;
    case LoggingLevel::VERBOSE:
        plog_level = logging::Level::VERBOSE;
        break;
    default:
        break;
    }
    // TODO: add a logging callback for serialize log
    logging::InitLogger(plog_level);
}

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