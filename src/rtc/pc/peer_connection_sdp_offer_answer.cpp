#include "rtc/pc/peer_connection.hpp"
#include "rtc/base/internals.hpp"
#include "rtc/sdp/sdp_entry.hpp"
#include "rtc/sdp/sdp_utils.hpp"
#include "rtc/transports/sctp_transport_internals.hpp"

#include <plog/Log.h>

#include <future>
#include <memory>

namespace naivertc {

// Offer && Answer
void PeerConnection::CreateOffer(SDPCreateSuccessCallback on_success, 
                                 SDPCreateFailureCallback on_failure) {
    signal_task_queue_->Async([this, on_success, on_failure](){
        try {
            if (this->signaling_state_ != SignalingState::HAVE_REMOTE_OFFER) {
                this->SetLocalDescription(sdp::Type::OFFER);
            }
            if (this->local_sdp_.has_value()) {
                auto local_sdp = this->local_sdp_.value();
                on_success(local_sdp);
            } else {
                throw std::runtime_error("Failed to create local offer sdp.");
            }
        }catch(const std::exception& exp) {
            on_failure(std::move(exp));
        }
    });
}

void PeerConnection::CreateAnswer(SDPCreateSuccessCallback on_success, 
                                  SDPCreateFailureCallback on_failure) {
    signal_task_queue_->Async([this, on_success, on_failure](){
        try {
            if (this->signaling_state_ == SignalingState::HAVE_REMOTE_OFFER) {
                this->SetLocalDescription(sdp::Type::ANSWER);
            }
            if (this->local_sdp_.has_value()) {
                auto local_sdp = this->local_sdp_.value();
                on_success(local_sdp);
            } else {
                throw std::runtime_error("Failed to create local answer sdp.");
            }
        }catch(const std::exception& exp) {
            on_failure(exp);
        }
    });
}

void PeerConnection::SetOffer(const std::string sdp,
                              SDPSetSuccessCallback on_success,
                              SDPSetFailureCallback on_failure) {
    signal_task_queue_->Async([this, sdp=std::move(sdp), on_success, on_failure](){
        try {
            auto remote_sdp = sdp::Description::Parser::Parse(sdp, sdp::Type::OFFER);
            this->SetRemoteDescription(std::move(remote_sdp));
            on_success();
        }catch(const std::exception& exp) {
            on_failure(exp);
        }
    });
}

void PeerConnection::SetAnswer(const std::string sdp, 
                                SDPSetSuccessCallback on_success, 
                                SDPSetFailureCallback on_failure) {
    signal_task_queue_->Async([this, sdp=std::move(sdp), on_success, on_failure](){
        try {
            auto remote_sdp = sdp::Description::Parser::Parse(sdp, sdp::Type::ANSWER);
            this->SetRemoteDescription(std::move(remote_sdp));
            on_success();
        }catch (const std::exception& exp) {
            on_failure(std::move(exp));
        }
    });        
}

void PeerConnection::AddRemoteCandidate(const std::string mid, const std::string sdp) {
    signal_task_queue_->Async([this, mid, sdp](){
        remote_candidates_.emplace_back(sdp::Candidate(sdp, mid));
        // Start to process remote candidate if the remote sdp is ready and the connection is not done yet.
        if (remote_sdp_ && connection_state_ != ConnectionState::CONNECTED) {
            ProcessRemoteCandidates();
        }
    });
}

// Private methods
void PeerConnection::SetLocalDescription(sdp::Type type) {
    RTC_RUN_ON(signal_task_queue_);
    if (connection_state_ == ConnectionState::CONNECTED || 
        connection_state_ == ConnectionState::CONNECTING) {
        throw std::logic_error("Unable to negotiate with remote peer when the local peer is " + ToString(connection_state_));
        return;
    }

    PLOG_VERBOSE << "Setting local description, type: " << type;

    if (type == sdp::Type::ROLLBACK) {
        if (signaling_state_ == SignalingState::HAVE_LOCAL_OFFER ||
            signaling_state_ == SignalingState::HAVE_LOCAL_PRANSWER) {
            // TODO: to rollback local sdp
            UpdateSignalingState(SignalingState::STABLE);
        }
        return;
    }

    // if the sdp type is unspecified
    if (type == sdp::Type::UNSPEC) {
        if (signaling_state_ == SignalingState::HAVE_REMOTE_OFFER) {
            type = sdp::Type::ANSWER;
        } else {
            type = sdp::Type::OFFER;
        }
    }

    // Only a local offer resets the negotiation needed flag
    if (type == sdp::Type::OFFER) {
        if (local_sdp_ && negotiation_needed_ == false) {
            PLOG_DEBUG << "No negotiation needed.";
            return;
        }
        negotiation_needed_ = false;
    }

    // Switch to new signaling state
    SignalingState new_signaling_state;
    switch (signaling_state_)
    {
    case SignalingState::STABLE: {
        // Stable means both the local and remote sdp not created yet, and here we need to create local sdp, and remote sdp is created by remote peer.
        if (type != sdp::Type::OFFER) {
            throw std::logic_error("Unexpected local sdp type: " + sdp::ToString(type) + " for signaling state: stable");
        }
        new_signaling_state = SignalingState::HAVE_LOCAL_OFFER;
        break;
    }
    case SignalingState::HAVE_REMOTE_OFFER:
    case SignalingState::HAVE_LOCAL_PRANSWER:
    {
        // Two situation:
        // 1. We have remote offer, and now we need to create a answer
        // 2. We have local pranswer, and now we need to recreate a pr-answer
        if (type != sdp::Type::ANSWER && 
            type != sdp::Type::PRANSWER) {
            throw std::logic_error("Unexpected local sdp type: " + sdp::ToString(type) + "for signling state: " + ToString(signaling_state_));
        }
        // Now we have both local and remote sdp. so the signaling state goes to stable.
        new_signaling_state = SignalingState::STABLE;
        break;
    }
    default:
        PLOG_WARNING << "Ignore unexpected local sdp type: " <<  type
                     << " in signaling state: " << signaling_state_;
        return;
    }

    // Retrieve the ICE SDP from ICE transport.
    auto local_ice_sdp = ice_transport_->GetLocalDescription(type);

    auto local_sdp_builder = sdp::Description::Builder(type);
    auto local_sdp = local_sdp_builder
                    .set_role(local_ice_sdp.role())
                    .set_ice_ufrag(local_ice_sdp.ice_ufrag())
                    .set_ice_pwd(local_ice_sdp.ice_pwd())
                    // Set local fingerprint (wait for certificate if necessary)
                    .set_fingerprint(certificate_.get()->fingerprint())
                    .Build();

    ProcessLocalDescription(local_sdp);

    UpdateSignalingState(new_signaling_state);  
}

void PeerConnection::SetRemoteDescription(sdp::Description remote_sdp) {
    RTC_RUN_ON(signal_task_queue_);
    if (connection_state_ == ConnectionState::CONNECTED || 
        connection_state_ == ConnectionState::CONNECTING) {
        throw std::logic_error("Unable to negotiate with remote peer when the local peer is " + ToString(connection_state_));
        return;
    }
    
    PLOG_VERBOSE << "Setting remote sdp: " << remote_sdp.type();

    // This is basically not gonna happen since we accept any offer
    if (remote_sdp.type() == sdp::Type::ROLLBACK) {
        PLOG_VERBOSE << "Rolling back pending remote sdp.";
        UpdateSignalingState(SignalingState::STABLE);
        return;
    }

    // To check if remote sdp is valid or not
    ValidRemoteDescription(remote_sdp);

    // Switch to new signaling state
    SignalingState new_signaling_state;
    switch (signaling_state_)
    {
    // If signaling state is stable, which means the local sdp is not create yet, so we assume the remote peer as offerer.
    case SignalingState::STABLE: {
        // TODO: Do we need to accept a remote pr-answer sdp in stable signaling state, not only the remote offer sdp?
        remote_sdp.HintType(sdp::Type::OFFER);
        if (remote_sdp.type() != sdp::Type::OFFER) {
            throw std::logic_error("Unexpected remote sdp type: " + sdp::ToString(remote_sdp.type()) + " in signaling state: stable");
        }
        new_signaling_state = SignalingState::HAVE_REMOTE_OFFER;
        break;
    }
    case SignalingState::HAVE_LOCAL_OFFER: {
        remote_sdp.HintType(sdp::Type::ANSWER);
        if (remote_sdp.type() != sdp::Type::ANSWER &&
            remote_sdp.type() != sdp::Type::PRANSWER) {
            throw std::logic_error("Unexpected remote sdp type: " + sdp::ToString(remote_sdp.type()) + " in signaling state: " + ToString(signaling_state_));
        }
        if (remote_sdp.type() == sdp::Type::OFFER) {
            // The ICE agent will intiate a rollback automatically when a peer had
            // pervoiusly created an offer receives an offer from the remote peer.
            SetLocalDescription(sdp::Type::ROLLBACK);
            new_signaling_state = SignalingState::HAVE_REMOTE_OFFER;
            break;
        }
        // Since we have both local and remote sdp now, the signaling state goes to stable.
        new_signaling_state = SignalingState::STABLE;
        break;
    }
    // If we already have a remote pr-answer sdp, we try to replace it with new remote sdp.
    case SignalingState::HAVE_REMOTE_PRANSWER: {
        remote_sdp.HintType(sdp::Type::ANSWER);
        if (remote_sdp.type() != sdp::Type::ANSWER &&
            remote_sdp.type() != sdp::Type::PRANSWER) {
            throw std::logic_error("Unexpected remote sdp type: " + sdp::ToString(remote_sdp.type()) + " in signaling state: " + ToString(signaling_state_));
        }
        new_signaling_state = SignalingState::STABLE;
        break;
    }
    default:
        // TODO: Do we need to accept a remote offer sdp in the HAVE_REMOTE_OFFER state, and repalce the old remote offer sdp with the new one?
        throw std::logic_error("Unexpected remote sdp type: " + sdp::ToString(remote_sdp.type()) + " in signaling state: " + ToString(signaling_state_));
    }

    ProcessRemoteDescription(std::move(remote_sdp));

    UpdateSignalingState(new_signaling_state);

    if (remote_sdp_) {
        // If this is an offer, we need to answer it
        if (remote_sdp_->type() == sdp::Type::OFFER &&
            rtc_config_.auto_negotiation) {
            SetLocalDescription(sdp::Type::ANSWER);
        }
        // Start to process remote candidate if remote sdp is ready
        ProcessRemoteCandidates();
    }
}

void PeerConnection::ProcessLocalDescription(sdp::Description& local_sdp) {
    RTC_RUN_ON(signal_task_queue_);
    const uint16_t local_sctp_port = rtc_config_.local_sctp_port.value_or(kDefaultSctpPort);
    const size_t local_max_message_size = rtc_config_.sctp_max_message_size.value_or(kDefaultSctpMaxMessageSize);

    // Clean up the application entry added by ICE transport already.
    local_sdp.ClearMediaEntries();

    // Reciprocate remote session description, 
    // e.g.: the local is answer and the remote is offer.
    if (auto remote = this->remote_sdp_) {
        // https://wanghenshui.github.io/2018/08/15/variant-visit
        if (auto remote_app = remote->application()) {
            // Need to create application for local data channels.
            if (data_channel_needed_) {
                sdp::Application local_app(remote_app->mid());
                local_app.set_sctp_port(local_sctp_port);
                local_app.set_max_message_size(local_max_message_size);
                
                PLOG_DEBUG << "Adding application to local description, mid= " << local_app.mid();

                local_sdp.SetApplication(std::move(local_app));

            } else {
                auto reciprocated = remote_app->ReciprocatedSDP();
                reciprocated.HintSctpPort(local_sctp_port);
                reciprocated.set_max_message_size(local_max_message_size);

                PLOG_DEBUG << "Reciprocating application in local description, mid: " 
                            << reciprocated.mid();

                local_sdp.SetApplication(std::move(reciprocated));
            }
        }
        remote->ForEach([this, &local_sdp](const sdp::Media& remote_media){
            // Prefer local media track
            // The local media track will override the remote media track with the same mid
            auto it = media_sdps_.find(remote_media.mid());
            if (it != media_sdps_.end()) {
                auto& local_media = it->second;
                PLOG_DEBUG << "Adding media to local description, mid=" << local_media.mid()
                           << ", active=" << std::boolalpha
                           << (local_media.direction() != sdp::Direction::INACTIVE);

                local_sdp.AddMedia(local_media);
            } else {
                auto reciprocated = remote_media.ReciprocatedSDP();
                PLOG_DEBUG << "Reciprocating media in local description, mid=" << reciprocated.mid()
                           << ", active=" << std::boolalpha
                           << (reciprocated.direction() != sdp::Direction::INACTIVE);;
                // Incoming media track with reciprocated SDP. 
                OnIncomingMediaTrack(reciprocated);
                local_sdp.AddMedia(std::move(reciprocated));
            }
            // local media track negotiated with remote
            OnMediaTrackNegotiated(remote_media);
        });

    } 

    if (local_sdp.type() == sdp::Type::OFFER) {
        // If this is a offer, add locally created data channels and tracks
        // The two conditions necessary for adding application entry:
        // 1. There is no one in local SDP yet.
        // 2. We have one or more data channels added by users
        // NOTE: All of data channels distiguished with stream id will use one SCTP connection for communication, 
        // that's why we just need to add one application here.
        if (!local_sdp.HasApplication() && data_channel_needed_) {
            // FIXME: Do we need to update data channle stream id here other than to shift it after received remote sdp later.
            // FIXED: No matter we are either DTLS client or server, we still need to create a data channel with mid started from 0,
            // since the data channel is owned by both of peers(the DTLS client and server). The only thing we need to do is to correct the mid of data channel 
            // added by user after the DTLS role of local peer was negotiated(After the remote sdp was processed by ICE transport).
            int new_mid = 0;
            while (local_sdp.HasMid(std::to_string(new_mid))) {
                ++new_mid;
            }
            sdp::Application app(std::to_string(new_mid));
            app.set_sctp_port(local_sctp_port);
            app.set_max_message_size(local_max_message_size);

            PLOG_DEBUG << "Adding application to local description, mid=" + app.mid();

            local_sdp.SetApplication(std::move(app));
        }

        // Add local media tracks
        for (auto& [mid, media] : media_sdps_) {
            // Filter existed tracks
            if (local_sdp.HasMid(mid)) {
                continue;
            }
            PLOG_DEBUG << "Adding media to local description, mid=" << media.mid()
                       << ", active=" << std::boolalpha
                       << (media.direction() != sdp::Direction::INACTIVE);

            local_sdp.AddMedia(media);
        }
    } 

    // TODO: Add candidates existed in old local sdp

    // Start to gather local candidate after local sdp was set.
    if (gathering_state_ == GatheringState::NEW) {
        PLOG_DEBUG << "Start to gather local candidates";
        ice_transport_->StartToGatherLocalCandidate(local_sdp.bundle_id());
    }

    // PLOG_VERBOSE << "Did process local sdp: " << std::string(local_sdp);

    local_sdp_ = std::move(local_sdp);
}
void PeerConnection::ProcessRemoteDescription(sdp::Description remote_sdp) {
    RTC_RUN_ON(signal_task_queue_);
    PLOG_VERBOSE << "Did process remote sdp: " << std::string(remote_sdp);

    // Handle incoming media track in remote SDP.
    if (remote_sdp.type() == sdp::Type::ANSWER) {
        remote_sdp.ForEach([this](const sdp::Media& remote_media) {
            auto it = media_sdps_.find(remote_media.mid());
            if (it == media_sdps_.end()) {
                auto reciprocated = remote_media.ReciprocatedSDP();
                PLOG_DEBUG << "Reciprocating media in local description, mid=" << reciprocated.mid()
                           << ", active=" << std::boolalpha
                           << (reciprocated.direction() != sdp::Direction::INACTIVE);;
                // Incoming media track with reciprocated SDP. 
                OnIncomingMediaTrack(reciprocated);
            }
            OnMediaTrackNegotiated(remote_media);
        });
    }

    auto remote_ice_sdp = IceTransport::Description(remote_sdp.type(), 
                                                    remote_sdp.role(), 
                                                    remote_sdp.ice_ufrag(), 
                                                    remote_sdp.ice_pwd());
    ice_transport_->SetRemoteDescription(std::move(remote_ice_sdp));

    remote_sdp_ = std::move(remote_sdp);
    
}

void PeerConnection::ProcessRemoteCandidates() {
    RTC_RUN_ON(signal_task_queue_);
    assert(remote_sdp_.has_value());
    for (auto candidate : remote_candidates_) {
        ProcessRemoteCandidate(std::move(candidate));
    }
    remote_candidates_.clear();
}

void PeerConnection::ProcessRemoteCandidate(sdp::Candidate candidate) {
    RTC_RUN_ON(signal_task_queue_);
    PLOG_VERBOSE << "Adding remote candidate: " << std::string(candidate);
    // We assume all medias are multiplex
    candidate.HintMid(remote_sdp_->bundle_id());
    candidate.Resolve(sdp::Candidate::ResolveMode::SIMPLE);

    // We might need a lookup
    if (candidate.isResolved() || candidate.Resolve(sdp::Candidate::ResolveMode::LOOK_UP)) {
        ice_transport_->AddRemoteCandidate(std::move(candidate));
    } else {
        PLOG_WARNING << "Failed to resolve remote candidate: " << std::string(candidate);
    }
}

void PeerConnection::ValidRemoteDescription(const sdp::Description& remote_sdp) {
    RTC_RUN_ON(signal_task_queue_);
    if (!remote_sdp.ice_ufrag()) {
        throw std::invalid_argument("Remote sdp has no ICE user fragment");
    }

    if (!remote_sdp.ice_pwd()) {
        throw std::invalid_argument("Remote sdp has no ICE password");
    }

    if (!remote_sdp.fingerprint()) {
        throw std::invalid_argument("Remote sdp has no valid fingerprint");
    }

    if (!remote_sdp.HasApplication() && !remote_sdp.HasMedia()) {
        throw std::invalid_argument("Remote sdp has no media line");
    }

    int active_media_count = remote_sdp.HasApplication() ? 1 : 0;
    remote_sdp.ForEach([&active_media_count](const sdp::Media& media){
        if (media.direction() != sdp::Direction::INACTIVE) {
            ++active_media_count;
        }
    });
   
    if (active_media_count == 0) {
        throw std::invalid_argument("Remote sdp has no active media");
    }

    if (local_sdp_) {
        if (local_sdp_->ice_ufrag() == remote_sdp.ice_ufrag() &&
            local_sdp_->ice_pwd() == remote_sdp.ice_pwd()) {
            throw std::logic_error("Got a local sdp as remote sdp");
        }
    }
}

void PeerConnection::OnIncomingMediaTrack(const sdp::Media& remote_sdp) {
    RTC_RUN_ON(signal_task_queue_);
    worker_task_queue_->Async([this, media_sdp=remote_sdp](){
        auto media_track = std::make_shared<MediaTrack>(std::move(media_sdp));
        // Make sure the current media track dosen't be added before.
        if (media_tracks_.find(media_track->mid()) == media_tracks_.end()) {
            media_tracks_.emplace(std::make_pair(media_track->mid(), media_track));
            if (media_track_callback_) {
                media_track_callback_(std::move(media_track));
            } else {
                pending_media_tracks_.push_back(std::move(media_track));
            }
        }
    });
}

} // namespace naivertc
