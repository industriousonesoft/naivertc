#include "pc/peer_connection.hpp"

#include <plog/Log.h>

namespace naivertc {

PeerConnection::PeerConnection(Configuration config) 
    : config_(config),
    connection_state_(ConnectionState::CLOSED)  {

    InitIceTransport();
}

PeerConnection::~PeerConnection() {

}

// private methods
void PeerConnection::InitIceTransport() {
    try {
       PLOG_VERBOSE << "Init Ice transport";

       ice_transport_.reset(new IceTransport(config_));

       ice_transport_->SignalStateChanged.connect(this, &PeerConnection::OnTransportStateChanged);
       ice_transport_->SignalGatheringStateChanged.connect(this, &PeerConnection::OnGatheringStateChanged);
       ice_transport_->SignalCandidateGathered.connect(this, &PeerConnection::OnCandidateGathered);

    } catch(const std::exception& e) {
        PLOG_ERROR << e.what();
        UpdateConnectionState(ConnectionState::FAILED);
        throw std::runtime_error("Ice tansport initialization failed.");
    }
    
}

bool PeerConnection::UpdateConnectionState(ConnectionState state) {
    if (connection_state_ == state) {
        return false;
    }
    connection_state_ = state;
    this->connection_state_callback_(state);
    return true;
}

bool PeerConnection::UpdateGatheringState(GatheringState state) {
    if (gathering_state_ == state) {
        return false;
    }
    gathering_state_ = state;
    this->gathering_state_callback_(state);
    return true;
}

// Ice State && Candidate
void PeerConnection::OnTransportStateChanged(Transport::State transport_state) {
    handle_queue_.Post([this, transport_state](){
        switch (transport_state) {
        case Transport::State::DISCONNECTED:
            this->UpdateConnectionState(ConnectionState::DISCONNECTED);
            break;
        case Transport::State::CONNECTING:
            this->UpdateConnectionState(ConnectionState::CONNECTING);
            break;
        case Transport::State::CONNECTED:
            this->UpdateConnectionState(ConnectionState::CONNECTED);
            break;
        case Transport::State::FAILED: 
            this->UpdateConnectionState(ConnectionState::FAILED);
            break;
        default:
            // Ignore the others state
            break;
        }
    });
}

void PeerConnection::OnGatheringStateChanged(IceTransport::GatheringState gathering_state) {
    handle_queue_.Post([this, gathering_state](){
        switch (gathering_state) {
        case IceTransport::GatheringState::NEW:
            this->UpdateGatheringState(GatheringState::NEW);
            break;
        case IceTransport::GatheringState::GATHERING:
            this->UpdateGatheringState(GatheringState::GATHERING);
            break;
        case IceTransport::GatheringState::COMPLETED:
            this->UpdateGatheringState(GatheringState::COMPLETED);
            break;
        default:
            break;
        }
    });
}

void PeerConnection::OnCandidateGathered(Candidate candidate) {
    handle_queue_.Post([this, candidate](){
        this->candidate_callback_(candidate);
    });
}

// Offer && Answer
void PeerConnection::CreateOffer(SDPSetSuccessCallback on_success, 
                SDPCreateFailureCallback on_failure) {
    handle_queue_.Post([this, on_success, on_failure](){
       
    });
}

void PeerConnection::CreateAnswer(SDPSetSuccessCallback on_success, 
                SDPCreateFailureCallback on_failure) {
    handle_queue_.Post([this, on_success, on_failure](){
        
    });
}

void PeerConnection::SetOffer(const std::string sdp,
            SDPSetSuccessCallback on_success,
            SDPSetFailureCallback on_failure) {
    handle_queue_.Post([this, sdp, on_success, on_failure](){
        
    });
}

void PeerConnection::SetAnswer(const std::string sdp, 
            SDPSetSuccessCallback on_success, 
            SDPSetFailureCallback on_failure) {
    handle_queue_.Post([this, sdp, on_success, on_failure](){
        
    });        
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
    
}