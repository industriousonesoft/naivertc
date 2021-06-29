#include "pc/peer_connection.hpp"

#include <plog/Log.h>

namespace naivertc {
// IceTransport slots
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

}