#include "pc/peer_connection.hpp"
#include "common/utils.hpp"
#include "base/internals.hpp"
#include "pc/sdp/sdp_entry.hpp"

#include <plog/Log.h>

#include <future>
#include <memory>

namespace naivertc {

void PeerConnection::SetLocalSessionDescription(sdp::SessionDescription session_description) {
    // Clean up the application entry added by ICE transport already.
    session_description.ClearMedia();                                

    const uint16_t local_sctp_port = DEFAULT_SCTP_PORT;
    const size_t local_max_message_size = config_.max_message_size_.value_or(DEFAULT_LOCAL_MAX_MESSAGE_SIZE);

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

                        AddRemoteTrack(reciprocated);

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
    local_session_description_.emplace(session_description);
   
    // TODO: Reciprocated tracks might need to be open

}
void PeerConnection::SetRemoteSessionDescription(sdp::SessionDescription session_description) {
    this->remote_session_description_.emplace(session_description);
    this->ice_transport_->SetRemoteDescription(session_description);

    // TODO: To shift datachannel? but why?

    // TODO: To init sctptransport if necessary
}

// Offer && Answer
void PeerConnection::CreateOffer(SDPCreateSuccessCallback on_success, 
                                    SDPCreateFailureCallback on_failure) {
    handle_queue_.Post([this, on_success, on_failure](){
        try {
            auto session_description = this->ice_transport_->GetLocalDescription(sdp::Type::OFFER);
            this->SetLocalSessionDescription(std::move(session_description));
            if (this->local_session_description_) {
                auto local_sdp = *this->local_session_description_;
                on_success(std::move(local_sdp));
            }else {
                throw std::runtime_error("Failed to create local offer sdp.");
            }
        }catch(...) {
            on_failure(std::current_exception());
        }
    });
}

void PeerConnection::CreateAnswer(SDPCreateSuccessCallback on_success, 
                                    SDPCreateFailureCallback on_failure) {
    handle_queue_.Post([this, on_success, on_failure](){
        try {
            auto session_description = this->ice_transport_->GetLocalDescription(sdp::Type::ANSWER);
            this->SetLocalSessionDescription(std::move(session_description));
            if (this->local_session_description_) {
                auto local_sdp = *this->local_session_description_;
                on_success(std::move(local_sdp));
            }else {
                throw std::runtime_error("Failed to create local answer sdp.");
            }
        }catch(...) {
            on_failure(std::current_exception());
        }
    });
}

void PeerConnection::SetOffer(const std::string sdp,
                                SDPSetSuccessCallback on_success,
                                SDPSetFailureCallback on_failure) {
    handle_queue_.Post([this, sdp = std::move(sdp), on_success, on_failure](){
        try {
            auto session_description = sdp::SessionDescription(sdp, sdp::Type::OFFER);
            this->SetRemoteSessionDescription(std::move(session_description));
            on_success();
        }catch(...) {
            on_failure(std::current_exception());
        }
    });
}

void PeerConnection::SetAnswer(const std::string sdp, 
                                SDPSetSuccessCallback on_success, 
                                SDPSetFailureCallback on_failure) {
    handle_queue_.Post([this, sdp = std::move(sdp), on_success, on_failure](){
        try {
            auto session_description = sdp::SessionDescription(sdp, sdp::Type::ANSWER);
            this->SetRemoteSessionDescription(std::move(session_description));
            on_success();
        }catch (...) {
            on_failure(std::current_exception());
        }
    });        
}

// Media Tracks
std::shared_ptr<MediaTrack> PeerConnection::AddTrack(const MediaTrack::Config& config) {
    std::promise<std::shared_ptr<MediaTrack>> promise;
    auto future = promise.get_future();
    handle_queue_.Post([this, init_config = std::move(config), &promise](){
        try {
            auto description = this->BuildMediaTrackDescription(std::move(init_config));

            std::shared_ptr<MediaTrack> media_track = nullptr;

            if (auto it = this->media_tracks_.find(description.mid()); it != this->media_tracks_.end()) {
                if (media_track = it->second.lock(), media_track) {
                    media_track->UpdateDescription(std::move(description));
                }
            }

            if (!media_track) {
                media_track = std::make_shared<MediaTrack>(std::move(description));
                this->media_tracks_.emplace(std::make_pair(media_track->mid(), media_track));
            }

            promise.set_value(media_track);
            // Renegotiation is needed for the new or updated track
            negotiation_needed_ = true;
        }catch (...) {
            promise.set_exception(std::current_exception());
        }
    });
    return future.get();
}

void PeerConnection::AddRemoteTrack(sdp::Media description) {
    if (media_tracks_.find(description.mid()) == media_tracks_.end()) {
        auto media_track = std::make_shared<MediaTrack>(std::move(description));
        media_tracks_.emplace(std::make_pair(media_track->mid(), media_track));
        // TODO: trigger track manually
    }
}

// Data Channels
std::shared_ptr<DataChannel> PeerConnection::CreateDataChannel(const DataChannel::Config& config) {
    std::promise<std::shared_ptr<DataChannel>> promise;
    auto future = promise.get_future();
    handle_queue_.Post([this, init_config = std::move(config), &promise](){
        StreamId stream_id;
        try {
            if (init_config.stream_id) {
                stream_id = *init_config.stream_id;
                if (stream_id > STREAM_ID_MAX_VALUE) {
                    throw std::invalid_argument("Invalid DataChannel stream id.");
                }
            }else {
                // RFC 5763: The answerer MUST use either a setup attibute value of setup:active or setup:passive.
                // and, setup::active is RECOMMENDED. See https://tools.ietf.org/html/rfc5763#section-5
                // Thus, we assume passive role if we are the offerer.
                auto role = this->ice_transport_->role();

                // FRC 8832: The peer that initiates opening a data channel selects a stream identifier for 
                // which the corresponding incoming and outgoing streams are unused. If the side is acting as the DTLS client,
                // it MUST choose an even stream identifier, if the side is acting as the DTLS server, it MUST choose an odd one.
                // See https://tools.ietf.org/html/rfc8832#section-6
                stream_id = (role == sdp::Role::ACTIVE) ? 0 : 1;
                // Avoid conflict with existed data channel
                while (data_channels_.find(stream_id) != data_channels_.end()) {
                    if (stream_id >= STREAM_ID_MAX_VALUE - 2) {
                        throw std::overflow_error("Too many DataChannels");
                    }
                    stream_id += 2;
                }
            }

            // We assume the DataChannel is not negotiacted
            auto data_channel = std::make_shared<DataChannel>(stream_id, init_config.label, init_config.protocol);
            data_channels_.emplace(std::make_pair(stream_id, data_channel));

            // TODO: To open channel if SCTP transport is created for now.

            // Renegotiation is needed if the curren local description does not have application
            if (local_session_description_ || local_session_description_->HasApplication() == false) {
                this->negotiation_needed_ = true;
            }

            if (this->config_.auto_negotiation) {
                // TODO: To negotiate automatically
            }

            promise.set_value(std::move(data_channel));
        }catch (...) {
            promise.set_exception(std::current_exception());
        }
    });
    return future.get();
}

// SDP Builder
sdp::Media PeerConnection::BuildMediaTrackDescription(const MediaTrack::Config& config) {
    auto codec = config.codec;
    auto kind = config.kind;
    if (kind == MediaTrack::Kind::VIDEO) {
        if (codec == MediaTrack::Codec::H264) {
            auto description = sdp::Video(config.mid);
            for (auto payload_type : config.payload_types) {
                description.AddCodec(payload_type, MediaTrack::codec_to_string(codec), MediaTrack::FormatProfileForPayloadType(payload_type));
            }
            return std::move(description);
        }else {
            throw std::invalid_argument("Unsupported video codec: " + MediaTrack::codec_to_string(codec));
        }
    }else if (kind == MediaTrack::Kind::AUDIO) {
        if (codec == MediaTrack::Codec::OPUS) {
            auto description = sdp::Audio(config.mid);
            for (int payload_type : config.payload_types) {
                // TODO: Not use fixed sample rate and channel value
                description.AddCodec(payload_type, MediaTrack::codec_to_string(codec), 48000, 2, MediaTrack::FormatProfileForPayloadType(payload_type));
            }
            return std::move(description);
        }else {
            throw std::invalid_argument("Unsupported audio codec: " + MediaTrack::codec_to_string(codec));
        }
    }else {
        throw std::invalid_argument("Unsupported media kind: " + MediaTrack::kind_to_string(kind));
    }
}
 

} // namespace naivertc
