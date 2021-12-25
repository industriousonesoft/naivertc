#include "rtc/pc/peer_connection.hpp"

#include <plog/Log.h>

namespace naivertc {
// Init IceTransport 
void PeerConnection::InitIceTransport() {
    RTC_RUN_ON(signaling_task_queue_);
    if (ice_transport_) {
        return;
    }
    PLOG_VERBOSE << "Init Ice transport";
    auto ice_config = IceTransport::Configuration();
    ice_config.ice_servers = rtc_config_.ice_servers;
    ice_config.enable_ice_tcp = rtc_config_.enable_ice_tcp;
    ice_config.port_range_begin = rtc_config_.port_range_begin;
    ice_config.port_range_end = rtc_config_.port_range_end;
#if USE_NICE
    ice_config.proxy_server = rtc_config_.proxy_server;
#else
    ice_config.bind_addresses = rtc_config_.bind_addresses;
#endif
    // RFC 5763: The answerer MUST use either a setup attibute value of setup:active or setup:passive.
    // and, setup::active is RECOMMENDED. See https://tools.ietf.org/html/rfc5763#section-5
    // Thus, we assume passive role if we are the offerer.
    sdp::Role role = sdp::Role::ACT_PASS;

    network_task_queue_->Async([this, role, config=std::move(ice_config)](){
        ice_transport_.reset(new IceTransport(std::move(config), role));
        
        ice_transport_->OnStateChanged(std::bind(&PeerConnection::OnIceTransportStateChanged, this, std::placeholders::_1));
        ice_transport_->OnGatheringStateChanged(std::bind(&PeerConnection::OnGatheringStateChanged, this, std::placeholders::_1));
        ice_transport_->OnCandidateGathered(std::bind(&PeerConnection::OnCandidateGathered, this, std::placeholders::_1));
        ice_transport_->OnRoleChanged(std::bind(&PeerConnection::OnRoleChanged, this, std::placeholders::_1));

        ice_transport_->Start(); 
    }); 
}

// IceTransport delegate
void PeerConnection::OnIceTransportStateChanged(Transport::State transport_state) {
    RTC_RUN_ON(network_task_queue_);
    signaling_task_queue_->Async([this, transport_state](){
        switch (transport_state) {
        case Transport::State::CONNECTING:
            UpdateConnectionState(ConnectionState::CONNECTING);
            break;
        case Transport::State::CONNECTED: {
            PLOG_DEBUG << "ICE transport connected";
            InitDtlsTransport();
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
    RTC_RUN_ON(network_task_queue_);
    signaling_task_queue_->Async([this, gathering_state](){
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
    RTC_RUN_ON(network_task_queue_);
    signaling_task_queue_->Async([this, candidate=std::move(candidate)](){
        if (this->candidate_callback_) {
            this->candidate_callback_(std::move(candidate));
        }
    });
}

void PeerConnection::OnRoleChanged(sdp::Role role) {
    RTC_RUN_ON(network_task_queue_);
    // If sctp transport is created already, which means we have no chance to change the role any more
    assert(sctp_transport_ == nullptr && "Can not change the DTLS role of data channel after SCTP transport was created.");
    signaling_task_queue_->Async([this, role](){
        // The role of DTLS is not changed (since we assumed as a DTLS server).
        if (role != sdp::Role::ACTIVE) {
            return;
        }
        // Since we assumed passive role during DataChannel creatation, we might need to 
        // shift the stream id of data channel from odd to even 
        ShiftDataChannelIfNeccessary(role);
    });
}

}