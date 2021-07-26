#include "rtc/pc/peer_connection.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

namespace naivertc {
// Init IceTransport 
void PeerConnection::InitIceTransport() {
    try {
       PLOG_VERBOSE << "Init Ice transport";
    
       ice_transport_.reset(new IceTransport(rtc_config_, network_task_queue_));
       
       ice_transport_->OnStateChanged(std::bind(&PeerConnection::OnIceTransportStateChanged, this, std::placeholders::_1));
       ice_transport_->OnGatheringStateChanged(std::bind(&PeerConnection::OnGatheringStateChanged, this, std::placeholders::_1));
       ice_transport_->OnCandidateGathered(std::bind(&PeerConnection::OnCandidateGathered, this, std::placeholders::_1));

       ice_transport_->Start();

    } catch(const std::exception& e) {
        PLOG_ERROR << "Failed to init ice transport: " << e.what();
        UpdateConnectionState(ConnectionState::FAILED);
        throw std::runtime_error("Ice tansport initialization failed.");
    }   
}

// IceTransport delegate
void PeerConnection::OnIceTransportStateChanged(Transport::State transport_state) {
    signal_task_queue_->Async([this, transport_state](){
        switch (transport_state) {
        case Transport::State::CONNECTING:
            this->UpdateConnectionState(ConnectionState::CONNECTING);
            break;
        case Transport::State::CONNECTED:
            PLOG_DEBUG << "ICE transport connected";
            this->InitDtlsTransport();
            break;
        case Transport::State::FAILED: 
            this->UpdateConnectionState(ConnectionState::FAILED);
            break;
        case Transport::State::DISCONNECTED:
            this->UpdateConnectionState(ConnectionState::DISCONNECTED);
            break;
        default:
            // Ignore the others state
            break;
        }
    });
}

void PeerConnection::OnGatheringStateChanged(IceTransport::GatheringState gathering_state) {
    signal_task_queue_->Async([this, gathering_state](){
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

void PeerConnection::OnCandidateGathered(sdp::Candidate candidate) {
    signal_task_queue_->Async([this, candidate = std::move(candidate)](){
        if (this->candidate_callback_) {
            this->candidate_callback_(std::move(candidate));
        }
    });
}

}