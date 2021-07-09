#include "pc/peer_connection.hpp"
#include "common/utils.hpp"
#include "base/internals.hpp"
#include "pc/sdp/sdp_entry.hpp"
#include "pc/media/h264_media_track.hpp"
#include "pc/media/opus_media_track.hpp"

#include <plog/Log.h>

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
                   // TODO: Prefer local data channel description

                   auto reciprocated = remote_app->reciprocate();
                   reciprocated.HintSctpPort(local_sctp_port);
                   reciprocated.set_max_message_size(local_max_message_size);

                   PLOG_DEBUG << "Reciprocating application in local description, mid: " 
                              << reciprocated.mid();

                   session_description.AddApplication(std::move(reciprocated));
                },
                [&](sdp::Media* remote_media) {
                    // TODO: Prefer local media track
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
void PeerConnection::AddTrack(std::shared_ptr<MediaTrack> media_track) {
    handle_queue_.Post([this, media_track](){
        // Create media description and add to local sdp
        auto kind = media_track->kind();
        auto codec = media_track->codec();
        if (kind == MediaTrack::Kind::VIDEO) {
            // H264 track
            if (codec == MediaTrack::Codec::H264 && utils::instanceof<H264MediaTrack>(media_track.get())) {
                auto h264_track = static_cast<H264MediaTrack*>(media_track.get());
                auto video_sdp_entry = std::make_shared<sdp::Video>(h264_track->mid());
                video_sdp_entry->AddCodec(h264_track->payload_type(), h264_track->codec_string(), h264_track->format_profile());
                this->media_sdp_entries_.emplace(std::make_pair(video_sdp_entry->mid(), video_sdp_entry));
            }else {
                PLOG_ERROR << "video track with codec: " << media_track->codec_string();
            }
        }else if(kind == MediaTrack::Kind::AUDIO) {
            // Opus track
            if (codec == MediaTrack::Codec::OPUS && utils::instanceof<OpusMediaTrack>(media_track.get())) {
                auto opus_track = static_cast<OpusMediaTrack*>(media_track.get());
                auto audio_sdp_entry = std::make_shared<sdp::Audio>(opus_track->mid());
                audio_sdp_entry->AddCodec(opus_track->payload_type(), opus_track->codec_string(), opus_track->sample_rate(), opus_track->channels(), opus_track->format_profile());
                this->media_sdp_entries_.emplace(std::make_pair(audio_sdp_entry->mid(), audio_sdp_entry));
            }else {
                PLOG_ERROR << "audio track with codec: " << media_track->codec_string();
            }
        }else {
            PLOG_ERROR << "Unsupported media track kind: " << media_track->kind_string() << ", codec: " << media_track->codec_string();
        }
    });
}

void PeerConnection::CreateDataChannel(DataChannelInit init) {
    handle_queue_.Post([this, init](){
        StreamId stream_id;
        if (init.stream_id) {
            stream_id = *init.stream_id;
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
                    throw std::runtime_error("Too many DataChannels");
                }
                stream_id += 2;
            }
        }

        // We assume the DataChannel is not negotiacted
        auto data_channel = std::make_shared<DataChannel>(stream_id, std::move(init.label), std::move(init.protocol));

        data_channels_.emplace(std::make_pair(stream_id, data_channel));
    });
}

} // namespace naivertc
