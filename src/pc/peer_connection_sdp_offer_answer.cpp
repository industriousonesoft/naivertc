#include "pc/peer_connection.hpp"
#include "common/utils.hpp"
#include "base/internals.hpp"
#include "pc/sdp/sdp_entry.hpp"
#include "pc/media/h264_media_track.hpp"
#include "pc/media/opus_media_track.hpp"

#include <plog/Log.h>

#include <future>
#include <memory>

namespace naivertc {

void PeerConnection::SetLocalSessionDescription(sdp::SessionDescription session_description, 
                                                SDPSetSuccessCallback on_success, 
                                                SDPSetFailureCallback on_failure) {
    this->local_session_description_.emplace(session_description);
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

                        PLOG_DEBUG << "Reciprocating application in local description, mid: " 
                                    << reciprocated.mid();

                        session_description.AddApplication(std::move(reciprocated));
                   }
                },
                [&](sdp::Media* remote_media) {
                    // TODO: Prefer local media track
                    if (auto it = media_tracks_.find(remote_media->mid()); it != media_tracks_.end()) {
                        if (auto local_media = it->second.lock()) {
                            // session_description.AddMedia(std::)
                        }
                    }


                    auto reciprocated = remote_media->reciprocate();

                    PLOG_DEBUG << "Reciprocating media in local description, mid: " 
                               << reciprocated.mid();
                    session_description.AddMedia(std::move(reciprocated));
                }
            }, remote->media(i));
        }
    } 

    if (session_description.type() == sdp::Type::OFFER) {
        // If this is a offer, add locally created data channels and tracks
        // Add application for data channels
    }      
}
void PeerConnection::SetRemoteSessionDescription(sdp::SessionDescription session_description, 
                                                SDPSetSuccessCallback on_success, 
                                                SDPSetFailureCallback on_failure) {
    this->remote_session_description_.emplace(session_description);
    this->ice_transport_->SetRemoteDescription(session_description);

    // TODO: To shift datachannel? but why?

    // TODO: To init sctptransport if necessary
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

// Media Tracks
std::shared_ptr<MediaTrack> PeerConnection::AddTrack(const MediaTrack::Config& config) {
    std::promise<std::shared_ptr<MediaTrack>> promise;
    auto future = promise.get_future();
    handle_queue_.Post([this, init_config = std::move(config), &promise](){
        try {
            // Create media description and add to local sdp
            auto codec = init_config.codec;
            auto kind = init_config.kind;
            std::shared_ptr<MediaTrack> media_track = nullptr;
            if (kind == MediaTrack::Kind::VIDEO) {
                // H264 track
                if (codec == MediaTrack::Codec::H264) {
                    auto h264_track = std::make_shared<H264MediaTrack>(std::move(init_config));
                    this->media_tracks_.emplace(std::make_pair(h264_track->mid(), h264_track));
                    media_track = std::move(h264_track);
                }else {
                    throw std::invalid_argument("Unsupported vodieo codec: " + MediaTrack::codec_to_string(codec));
                }
            }else if(kind == MediaTrack::Kind::AUDIO) {
                // Opus track
                if (codec == MediaTrack::Codec::OPUS) {
                    auto opus_track = std::make_shared<OpusMediaTrack>(std::move(init_config), 48000, 2);
                    this->media_tracks_.emplace(std::make_pair(opus_track->mid(), opus_track));
                    media_track = std::move(opus_track);
                }else {
                    throw std::invalid_argument("Unsupported audio codec: " + MediaTrack::codec_to_string(codec));
                }
            }else {
                throw std::invalid_argument("Unsupported media kind: " + MediaTrack::kind_to_string(kind));
            }
            promise.set_value(media_track);
        }catch (...) {
            promise.set_exception(std::current_exception());
        }
    });
    return future.get();
}

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

            promise.set_value(std::move(data_channel));

        }catch (...) {
            promise.set_exception(std::current_exception());
        }
    });
    return future.get();
}

} // namespace naivertc
