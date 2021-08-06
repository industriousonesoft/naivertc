#include "rtc/pc/peer_connection.hpp"

#include <plog/Log.h>

namespace naivertc {
    
std::shared_ptr<MediaTrack> PeerConnection::AddTrack(const MediaTrack::Config config) {
    return signal_task_queue_->Sync<std::shared_ptr<MediaTrack>>([this, config = std::move(config)]() -> std::shared_ptr<MediaTrack> {
        try {
            auto description = MediaTrack::BuildDescription(std::move(config));

            std::shared_ptr<MediaTrack> media_track = nullptr;

            if (auto it = this->media_tracks_.find(description.mid()); it != this->media_tracks_.end()) {
                if (media_track = it->second, media_track) {
                    media_track->UpdateDescription(std::move(description));
                }
            }

            if (!media_track) {
                media_track = std::make_shared<MediaTrack>(std::move(description));
                this->media_tracks_.emplace(std::make_pair(media_track->mid(), media_track));
            }

            // Renegotiation is needed for the new or updated track
            negotiation_needed_ = true;

            return media_track;

        }catch (const std::exception& exp) {
            PLOG_ERROR << "Failed to add media track: " << exp.what();
            return nullptr;
        }
    });
}

void PeerConnection::AddReciprocatedMediaTrack(sdp::Media description) {
    if (media_tracks_.find(description.mid()) == media_tracks_.end()) {
        auto media_track = std::make_shared<MediaTrack>(std::move(description));
        media_tracks_.emplace(std::make_pair(media_track->mid(), media_track));
        // TODO: trigger track manually
    }
}

// Data Channels
std::shared_ptr<DataChannel> PeerConnection::CreateDataChannel(const DataChannel::Init config) {
    return signal_task_queue_->Sync<std::shared_ptr<DataChannel>>([this, init_config = std::move(config)]() -> std::shared_ptr<DataChannel> {
        StreamId stream_id;
        try {
            if (init_config.stream_id) {
                stream_id = *init_config.stream_id;
                if (stream_id > kStreamIdMaxValue) {
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
                // The stream id is not equvalent to the mid of application in SDP, which is only used to distinguish the data channel and DTLS role.
                stream_id = (role == sdp::Role::ACTIVE) ? 0 : 1;
                // Avoid conflict with existed data channel
                while (data_channels_.find(stream_id) != data_channels_.end()) {
                    if (stream_id >= kStreamIdMaxValue - 2) {
                        throw std::overflow_error("Too many DataChannels");
                    }
                    stream_id += 2;
                }
            }

            // We assume the DataChannel is not negotiacted
            auto data_channel = std::make_shared<DataChannel>(init_config.label, init_config.protocol, stream_id, init_config.unordered);
    
            // If sctp transport is connected yet, we open the data channel immidiately
            if (sctp_transport_ && sctp_transport_->state() == SctpTransport::State::CONNECTED) {
                data_channel->AttachTo(sctp_transport_);
                data_channel->Open();
            }

            // Renegotiation is needed if the curren local description does not have application
            if (!local_sdp_ || local_sdp_->HasApplication() == false) {
                this->negotiation_needed_ = true;
            }

            data_channels_.emplace(stream_id, data_channel);
            return data_channel;

        }catch (const std::exception& exp) {
            PLOG_ERROR << "Failed to add media track: " << exp.what();
            return nullptr;
        }
    });
}

} // namespace naivertc
