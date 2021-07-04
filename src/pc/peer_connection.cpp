#include "pc/peer_connection.hpp"
#include "common/utils.hpp"
#include "base/internals.hpp"

#include <plog/Log.h>

#include <variant>
#include <string>

namespace naivertc {

PeerConnection::PeerConnection(Configuration config) 
    : config_(config),
    connection_state_(ConnectionState::CLOSED)  {

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

void PeerConnection::InitSctpTransport() {
    try {

    } catch(const std::exception& exp) {

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

void PeerConnection::SetLocalSessionDescription(sdp::SessionDescription session_description, 
                                                SDPSetSuccessCallback on_success, 
                                                SDPSetFailureCallback on_failure) {
    this->local_session_description_.emplace(session_description);
    // Clean up the application entry added by ICE transport already.
    session_description.ClearMedia();                                

    // const uint16_t local_sctp_port = DEFAULT_SCTP_PORT;
    // const size_t local_max_message_size = config_.max_message_size_.value_or(DEFAULT_LOCAL_MAX_MESSAGE_SIZE);

    // Reciprocate remote session description
    if (auto remote = this->remote_session_description_) {
        // https://wanghenshui.github.io/2018/08/15/variant-visit
        for (unsigned int i = 0; i < remote->media_count(); ++i) {
            std::visit(utils::overloaded {
                [&](sdp::Application* remote_app) {
                   
                },
                [&](sdp::Media* remote_media) {

                }
            }, remote->media(i));
        }
    }       
}
void PeerConnection::SetRemoteSessionDescription(sdp::SessionDescription session_description, 
                                                SDPSetSuccessCallback on_success, 
                                                SDPSetFailureCallback on_failure) {
    this->remote_session_description_.emplace(session_description);
    this->ice_transport_->SetRemoteDescription(session_description);
    // TODO: shift datachannel? why?
}

// Offer && Answer
void PeerConnection::CreateOffer(SDPSetSuccessCallback on_success, 
                SDPCreateFailureCallback on_failure) {
    handle_queue_.Post([this, on_success, on_failure](){
       auto session_description = this->ice_transport_->GetLocalDescription(sdp::Type::OFFER);
       this->SetLocalSessionDescription(std::move(session_description));
    });
}

void PeerConnection::CreateAnswer(SDPSetSuccessCallback on_success, 
                SDPCreateFailureCallback on_failure) {
    handle_queue_.Post([this, on_success, on_failure](){
        auto session_description = this->ice_transport_->GetLocalDescription(sdp::Type::ANSWER);
        this->SetLocalSessionDescription(std::move(session_description));
    });
}

void PeerConnection::SetOffer(const std::string sdp,
            SDPSetSuccessCallback on_success,
            SDPSetFailureCallback on_failure) {
    handle_queue_.Post([this, sdp, on_success, on_failure](){
        auto session_description = sdp::SessionDescription(sdp, sdp::Type::OFFER);
        this->SetRemoteSessionDescription(std::move(session_description), on_success, on_failure);
    });
}

void PeerConnection::SetAnswer(const std::string sdp, 
            SDPSetSuccessCallback on_success, 
            SDPSetFailureCallback on_failure) {
    handle_queue_.Post([this, sdp, on_success, on_failure](){
        auto session_description = sdp::SessionDescription(sdp, sdp::Type::ANSWER);
        this->SetRemoteSessionDescription(std::move(session_description), on_success, on_failure);
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