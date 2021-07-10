#include "pc/peer_connection.hpp"

namespace naivertc {
    
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

} // namespace naivertc
