#include "pc/peer_connection.hpp"
#include "common/utils.hpp"
#include "base/internals.hpp"
#include "pc/sdp/sdp_entry.hpp"

#include <plog/Log.h>

#include <future>
#include <memory>

namespace naivertc {

// Offer && Answer
void PeerConnection::CreateOffer(SDPCreateSuccessCallback on_success, 
                                    SDPCreateFailureCallback on_failure) {
    handle_queue_.Post([this, on_success, on_failure](){
        try {
            this->SetLocalDescription(sdp::Type::OFFER);
            if (this->local_session_description_.has_value()) {
                auto local_sdp = this->local_session_description_.value();
                on_success(std::move(local_sdp));
            }else {
                throw std::runtime_error("Failed to create local offer sdp.");
            }
        }catch(const std::exception& exp) {
            on_failure(std::move(exp));
        }
    });
}

void PeerConnection::CreateAnswer(SDPCreateSuccessCallback on_success, 
                                    SDPCreateFailureCallback on_failure) {
    handle_queue_.Post([this, on_success, on_failure](){
        try {
            this->SetLocalDescription(sdp::Type::ANSWER);
            if (this->local_session_description_.has_value()) {
                auto local_sdp = this->local_session_description_.value();
                on_success(std::move(local_sdp));
            }else {
                throw std::runtime_error("Failed to create local answer sdp.");
            }
        }catch(const std::exception& exp) {
            on_failure(std::move(exp));
        }
    });
}

void PeerConnection::SetOffer(const std::string sdp,
                                SDPSetSuccessCallback on_success,
                                SDPSetFailureCallback on_failure) {
    handle_queue_.Post([this, sdp = std::move(sdp), on_success, on_failure](){
        try {
            auto remote_sdp = sdp::SessionDescription(sdp, sdp::Type::OFFER);
            this->SetRemoteDescription(std::move(remote_sdp));
            on_success();
        }catch(const std::exception& exp) {
            on_failure(std::move(exp));
        }
    });
}

void PeerConnection::SetAnswer(const std::string sdp, 
                                SDPSetSuccessCallback on_success, 
                                SDPSetFailureCallback on_failure) {
    handle_queue_.Post([this, sdp = std::move(sdp), on_success, on_failure](){
        try {
            auto remote_sdp = sdp::SessionDescription(sdp, sdp::Type::ANSWER);
            this->SetRemoteDescription(std::move(remote_sdp));
            on_success();
        }catch (const std::exception& exp) {
            on_failure(std::move(exp));
        }
    });        
}

void PeerConnection::AddRemoteCandidate(const Candidate& candidate) {
    handle_queue_.Post([this, candidate = std::move(candidate)](){

        remote_candidates_.emplace_back(std::move(candidate));

        // Start to process remote candidate if remote sdp is ready
        if (remote_session_description_) {
            ProcessRemoteCandidates();
        }
    });
}

// SDP Processor
void PeerConnection::SetLocalDescription(sdp::Type type) {
    PLOG_VERBOSE << "Setting local description, type: " << sdp::TypeToString(type);

    if (type == sdp::Type::ROLLBACK) {
        if (signaling_state_ == SignalingState::HAVE_LOCAL_OFFER ||
            signaling_state_ == SignalingState::HAVE_LOCAL_PRANSWER) {
            // TODO: to rollbak local sdp
            UpdateSignalingState(SignalingState::STABLE);
        }
        return;
    }

    // if the sdp type is unspecified
    if (type == sdp::Type::UNSPEC) {
        if (signaling_state_ == SignalingState::HAVE_REMOTE_OFFER) {
            type = sdp::Type::ANSWER;
        }else {
            type = sdp::Type::OFFER;
        }
    }

    // Only a local offer resets the negotiation needed flag
    if (type == sdp::Type::OFFER) {
        if (local_session_description_ && negotiation_needed_ == false) {
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
            throw std::logic_error("Unexpected local sdp type: " + sdp::TypeToString(type) + " for signaling state: stable");
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
        if (type != sdp::Type::ANSWER && type != sdp::Type::PRANSWER) {
            throw std::logic_error("Unexpected local sdp type: " + sdp::TypeToString(type) + "for signling state: " + signaling_state_to_string(signaling_state_));
        }
        // Now we have both local and remote sdp. so the signaling state goes to stable.
        new_signaling_state = SignalingState::STABLE;
        break;
    }
    default:
        PLOG_WARNING << "Ignore unexpected local sdp type: " <<  sdp::TypeToString(type) << " in signaling state: " << signaling_state_to_string(signaling_state_);
        return;
    }

    auto session_description = ice_transport_->GetLocalDescription(type);
    ProcessLocalDescription(std::move(session_description));

    UpdateSignalingState(new_signaling_state);

    // Start to gather local candidate after local sdp was set.
    if (gathering_state_ == GatheringState::NEW && local_session_description_) {
        ice_transport_->GatheringLocalCandidate(local_session_description_->bundle_id());
    }
}

void PeerConnection::SetRemoteDescription(sdp::SessionDescription description) {
    PLOG_VERBOSE << "Setting remote sdp: " << sdp::TypeToString(description.type());

    // This is basically not gonna happen since we accept any offer
    if (description.type() == sdp::Type::ROLLBACK) {
        PLOG_VERBOSE << "Rolling back pending remote sdp.";
        UpdateSignalingState(SignalingState::STABLE);
        return;
    }

    // To check if remote sdp is valid or not
    ValidRemoteDescription(description);

    // Switch to new signaling state
    SignalingState new_signaling_state;
    switch (signaling_state_)
    {
    // If signaling state is stable, which means the local sdp is not create yet, so we assume the remote peer as offerer.
    case SignalingState::STABLE: {
        // TODO: Do we need to accept a remote pr-answer sdp in stable signaling state, not only the remote offer sdp?
        description.hintType(sdp::Type::OFFER);
        if (description.type() != sdp::Type::OFFER) {
            throw std::logic_error("Unexpected remote sdp type: " + sdp::TypeToString(description.type()) + " in signaling state: stable");
        }
        new_signaling_state = SignalingState::HAVE_REMOTE_OFFER;
        break;
    }
    case SignalingState::HAVE_LOCAL_OFFER: {
        description.hintType(sdp::Type::ANSWER);
        if (description.type() != sdp::Type::ANSWER ||
            description.type() != sdp::Type::PRANSWER) {
            throw std::logic_error("Unexpected remote sdp type: " + sdp::TypeToString(description.type()) + " in signaling state: " + signaling_state_to_string(signaling_state_));
        }
        if (description.type() == sdp::Type::OFFER) {
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
        description.hintType(sdp::Type::ANSWER);
        if (description.type() != sdp::Type::ANSWER ||
            description.type() != sdp::Type::PRANSWER) {
            throw std::logic_error("Unexpected remote sdp type: " + sdp::TypeToString(description.type()) + " in signaling state: " + signaling_state_to_string(signaling_state_));
        }
        new_signaling_state = SignalingState::STABLE;
        break;
    }
    default:
        // TODO: Do we need to accept a remote offer sdp in the HAVE_REMOTE_OFFER state, and repalce the old remote offer sdp with the new one?
        throw std::logic_error("Unexpected remote sdp type: " + sdp::TypeToString(description.type()) + " in signaling state: " + signaling_state_to_string(signaling_state_));
    }

    ProcessRemoteDescription(std::move(description));

    UpdateSignalingState(new_signaling_state);

    // If this is an offer, we need to answer it
    if (remote_session_description_ && remote_session_description_->type() == sdp::Type::OFFER && rtc_config_.auto_negotiation) {
        SetLocalDescription(sdp::Type::ANSWER);
    }

    // Start to process remote candidate if remote sdp is ready
    ProcessRemoteCandidates();
}

void PeerConnection::ProcessLocalDescription(sdp::SessionDescription session_description) {
    const uint16_t local_sctp_port = DEFAULT_SCTP_PORT;
    const size_t local_max_message_size = rtc_config_.max_message_size.value_or(DEFAULT_LOCAL_MAX_MESSAGE_SIZE);

    // Clean up the application entry added by ICE transport already.
    session_description.ClearMedia();  

    // Reciprocate remote session description
    if (auto remote = this->remote_session_description_) {
        // https://wanghenshui.github.io/2018/08/15/variant-visit
        for (unsigned int i = 0; i < remote->media_count(); ++i) {
            std::visit(utils::overloaded {
                [&](sdp::Application* remote_app) {
                    // Prefer local description
                   if (!data_channels_.empty()) {
                        sdp::Application local_app(remote_app->mid());
                        local_app.set_sctp_port(local_sctp_port);
                        local_app.set_max_message_size(local_max_message_size);
                       
                        PLOG_DEBUG << "Adding application to local description, mid= " << local_app.mid();

                        session_description.AddApplication(std::move(local_app));

                   }else {
                        auto reciprocated = remote_app->reciprocate();
                        reciprocated.HintSctpPort(local_sctp_port);
                        reciprocated.set_max_message_size(local_max_message_size);

                        PLOG_DEBUG 
                            << "Reciprocating application in local description, mid: " 
                            << reciprocated.mid();

                        session_description.AddApplication(std::move(reciprocated));
                   }
                },
                [&](sdp::Media* remote_media) {
                    // Prefer local media track
                    if (auto it = media_tracks_.find(remote_media->mid()); it != media_tracks_.end()) {
                        if (auto local_track = it->second.lock()) {
                            // 此处调用的是拷贝构造函数
                            auto local_media = local_track->description();
                            PLOG_DEBUG << "Adding media to local description, mid=" << local_media.mid()
                                        << ", active=" << std::boolalpha
                                        << (local_media.direction() != sdp::Direction::INACTIVE);

                            session_description.AddMedia(std::move(local_media));
                        // Local track was removed
                        }else {
                            auto reciprocated = remote_media->reciprocate();
                            reciprocated.set_direction(sdp::Direction::INACTIVE);

                            PLOG_DEBUG << "Adding inactive media to local description, mid=" << reciprocated.mid();

                            session_description.AddMedia(std::move(reciprocated));
                        }
                    }else {
                        auto reciprocated = remote_media->reciprocate();

                        AddReciprocatedMediaTrack(reciprocated);

                        PLOG_DEBUG << "Reciprocating media in local description, mid: " 
                                << reciprocated.mid();
                        session_description.AddMedia(std::move(reciprocated));
                    }
                }
            }, remote->media(i));
        }
    } 

    if (session_description.type() == sdp::Type::OFFER) {
        // If this is a offer, add locally created data channels and tracks
        // Add application for data channels
        if (session_description.HasApplication() == false) {
            if (data_channels_.empty() == false) {
                StreamId new_mid = 0;
                while (session_description.HasMid(std::to_string(new_mid))) {
                    ++new_mid;
                }
                // FIXME: Do we need to update data channle stream id here other than to shift it after received remote sdp later.
                sdp::Application app(std::to_string(new_mid));
                app.set_sctp_port(local_sctp_port);
                app.set_max_message_size(local_max_message_size);

                PLOG_DEBUG << "Adding application to local description, mid=" + app.mid();

                session_description.AddApplication(std::move(app));
            }
        }

        // Add media for local tracks
        for (auto it = media_tracks_.begin(); it != media_tracks_.end(); ++it) {
            if (auto track = it->second.lock()) {
                // Filter existed tracks
                if (session_description.HasMid(track->mid())) {
                    continue;
                }
                auto media = track->description();

                PLOG_DEBUG << "Adding media to local description, mid=" << media.mid()
                            << ", active=" << std::boolalpha
                            << (media.direction() != sdp::Direction::INACTIVE);

                session_description.AddMedia(std::move(media));
            }
        }
    } 

    // Set local fingerprint (wait for certificate if necessary)
    session_description.set_fingerprint(certificate_.get()->fingerprint());

    // TODO: Add candidates existed in old local sdp

    PLOG_VERBOSE << "Did set local sdp: " << std::string(session_description);

    local_session_description_ = std::move(session_description);
   
    // TODO: Reciprocated tracks might need to be open

}
void PeerConnection::ProcessRemoteDescription(sdp::SessionDescription session_description) {
    
    ice_transport_->SetRemoteDescription(session_description);

    // Since we assumed passive role during DataChannel creatation, we might need to 
    // shift the stream id form odd to even
    ShiftDataChannelIfNeccessary();

    // If both the local and remote sdp have application, we need to create sctp transport for data channel
    if (session_description.HasApplication()) {
        if (!sctp_transport_ && dtls_transport_ && dtls_transport_->state() == Transport::State::CONNECTED) {
            InitSctpTransport();
        }
    }

    remote_session_description_ = std::move(session_description);
}

void PeerConnection::ProcessRemoteCandidates() {
    for (const auto &candidate : remote_candidates_) {
        ProcessRemoteCandidate(std::move(candidate));
    }
    remote_candidates_.clear();
}

void PeerConnection::ProcessRemoteCandidate(Candidate candidate) {
    PLOG_VERBOSE << "Adding remote candidate: " << std::string(candidate);

    if (!remote_session_description_) {
        throw std::logic_error("Failed to process remote candidate without remote sdp");
    }

    if (!ice_transport_) {
        throw std::logic_error("Failed to process remote candidate without ICE transport");
    }

    // We assume all medias are multiplex
    candidate.HintMid(remote_session_description_->bundle_id());
    candidate.Resolve(Candidate::ResolveMode::SIMPLE);

    if (candidate.isResolved()) {
        ice_transport_->AddRemoteCandidate(std::move(candidate));
    // We might need a lookup
    }else if (candidate.Resolve(Candidate::ResolveMode::LOOK_UP)) {
        ice_transport_->AddRemoteCandidate(std::move(candidate));
    }else {
        throw std::runtime_error("Failed to resolve remote candidate");
    }

}

void PeerConnection::ValidRemoteDescription(const sdp::SessionDescription& session_description) {
    if (!session_description.ice_ufrag()) {
        throw std::invalid_argument("Remote sdp has no ICE user fragment");
    }

    if (!session_description.ice_pwd()) {
        throw std::invalid_argument("Remote sdp has no ICE password");
    }

    if (!session_description.fingerprint()) {
        throw std::invalid_argument("Remote sdp has no valid fingerprint");
    }

    if (!session_description.media_count()) {
        throw std::invalid_argument("Remote sdp has no media line");
    }

    int active_media_count = 0;
    for(unsigned int i = 0; i < session_description.media_count(); ++i ) {
        std::visit(utils::overloaded{
            [&](const sdp::Application*) {
                ++active_media_count;
            },
            [&](const sdp::Media* media) {
                if (media->direction() != sdp::Direction::INACTIVE) {
                    ++active_media_count;
                }
            }
        }, session_description.media(i));
    }

    if (active_media_count == 0) {
        throw std::invalid_argument("Remote sdp has no active media");
    }

    if (local_session_description_) {
        if (local_session_description_->ice_ufrag() == session_description.ice_ufrag() &&
            local_session_description_->ice_pwd() == session_description.ice_pwd()) {
            throw std::logic_error("Got a local sdp as remote sdp");
        }
    }
}

void PeerConnection::ShiftDataChannelIfNeccessary() {
    // If sctp transport was created which means we have no chance to change the role any more
    // or ice transport does not acts as active role cause we assumed we are passive role at first
    if (sctp_transport_ && ice_transport_ && ice_transport_->role() != sdp::Role::ACTIVE) {
        return;
    }

    // We need to update the mid of data channel as a active 
    for (auto it = data_channels_.begin(); it != data_channels_.end(); ++it) {
        if (auto data_channel = it->second.lock()) {
            data_channel.get()->HintStreamIdForRole(sdp::Role::ACTIVE);
        }
    }

}

} // namespace naivertc
