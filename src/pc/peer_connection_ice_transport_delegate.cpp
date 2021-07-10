#include "pc/peer_connection.hpp"

#include <plog/Log.h>

namespace naivertc {
// Init IceTransport 
void PeerConnection::InitIceTransport() {
    try {
       PLOG_VERBOSE << "Init Ice transport";

       ice_transport_.reset(new IceTransport(config_));

       ice_transport_->SignalStateChanged.connect(this, &PeerConnection::OnIceTransportStateChanged);
       ice_transport_->SignalGatheringStateChanged.connect(this, &PeerConnection::OnGatheringStateChanged);
       ice_transport_->SignalCandidateGathered.connect(this, &PeerConnection::OnCandidateGathered);

    } catch(const std::exception& e) {
        PLOG_ERROR << e.what();
        UpdateConnectionState(ConnectionState::FAILED);
        throw std::runtime_error("Ice tansport initialization failed.");
    }   
}

// IceTransport delegate
void PeerConnection::OnIceTransportStateChanged(Transport::State transport_state) {
    handle_queue_.Post([this, transport_state](){
        switch (transport_state) {
        case Transport::State::DISCONNECTED:
            this->UpdateConnectionState(ConnectionState::DISCONNECTED);
            break;
        case Transport::State::CONNECTING:
            this->UpdateConnectionState(ConnectionState::CONNECTING);
            break;
        case Transport::State::CONNECTED:
            // TODO: Init DTLS transport
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
    handle_queue_.Post([this, candidate = std::move(candidate)](){
        this->candidate_callback_(std::move(candidate));
    });
}

}