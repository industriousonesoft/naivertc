#include "rtc/pc/peer_connection.hpp"

#include <plog/Log.h>

namespace naivertc {
// Init IceTransport 
void PeerConnection::InitIceTransport(RtcConfiguration config, sdp::Role role) {
    assert(network_task_queue_->IsCurrent());
    try {
        if (ice_transport_) {
           return;
        }
        PLOG_VERBOSE << "Init Ice transport";
    
       ice_transport_.reset(new IceTransport(std::move(config), role));
       
       ice_transport_->OnStateChanged(std::bind(&PeerConnection::OnIceTransportStateChanged, this, std::placeholders::_1));
       ice_transport_->OnGatheringStateChanged(std::bind(&PeerConnection::OnGatheringStateChanged, this, std::placeholders::_1));
       ice_transport_->OnCandidateGathered(std::bind(&PeerConnection::OnCandidateGathered, this, std::placeholders::_1));
       ice_transport_->OnRoleChanged(std::bind(&PeerConnection::OnRoleChanged, this, std::placeholders::_1));

       ice_transport_->Start();

    } catch(const std::exception& e) {
        PLOG_ERROR << "Failed to init ice transport: " << e.what();
        UpdateConnectionState(ConnectionState::FAILED);
        throw std::runtime_error("Ice tansport initialization failed.");
    }   
}

// IceTransport delegate
void PeerConnection::OnIceTransportStateChanged(Transport::State transport_state) {
    assert(network_task_queue_->IsCurrent());
    signal_task_queue_->Async([this, transport_state](){
        switch (transport_state) {
        case Transport::State::CONNECTING:
            UpdateConnectionState(ConnectionState::CONNECTING);
            break;
        case Transport::State::CONNECTED: {
            PLOG_DEBUG << "ICE transport connected";
            auto dtls_config = CreateDtlsConfig();
            network_task_queue_->Async([this, config=std::move(dtls_config)](){
                InitDtlsTransport(std::move(config));
            });
            break;
        }
        case Transport::State::FAILED: 
            UpdateConnectionState(ConnectionState::FAILED);
            PLOG_DEBUG << "ICE transport failed";
            break;
        case Transport::State::DISCONNECTED:
            UpdateConnectionState(ConnectionState::DISCONNECTED);
            PLOG_DEBUG << "ICE transport disconnected";
            break;
        default:
            // Ignore the others state
            break;
        }
    });
}

void PeerConnection::OnGatheringStateChanged(IceTransport::GatheringState gathering_state) {
    assert(network_task_queue_->IsCurrent());
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
    assert(network_task_queue_->IsCurrent());
    signal_task_queue_->Async([this, candidate = std::move(candidate)](){
        if (this->candidate_callback_) {
            this->candidate_callback_(std::move(candidate));
        }
    });
}

void PeerConnection::OnRoleChanged(sdp::Role role) {
    assert(network_task_queue_->IsCurrent());
    // If sctp transport is created already, which means we have no chance to change the role any more
    assert(sctp_transport_ == nullptr && "Can not change the DTLS role of data channel after SCTP transport was created.");
    worker_task_queue_->Async([this, role](){
        role_ = role;
        // The role of DTLS is not changed(since we assumed as a DTLS server)
        if (role != sdp::Role::ACTIVE) {
            return;
        }
        // Since we assumed passive role during DataChannel creatation, we might need to 
        // shift the stream id of data channel from odd to even 
        ShiftDataChannelIfNeccessary(role);
    });
}

}