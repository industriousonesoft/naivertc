#include "rtc/pc/peer_connection.hpp"
#include "rtc/transports/sctp_transport_internals.hpp"

#include <plog/Log.h>

namespace naivertc {
    
std::shared_ptr<AudioTrack> PeerConnection::AddAudioTrack(const MediaTrack::Configuration& config) {
    return signaling_task_queue_->Invoke<std::shared_ptr<AudioTrack>>([this, &config]() -> std::shared_ptr<AudioTrack> {
        std::shared_ptr<MediaTrack> media_track = FindMediaTrack(config.mid());
        if (!media_track) {
            media_track = std::make_shared<AudioTrack>(config, &broadcaster_, worker_task_queue_.get());
            this->media_tracks_.emplace(std::make_pair(media_track->mid(), media_track));
        } else {
            PLOG_WARNING << "The media track ["
                         << "kind = " << config.kind()
                         << ", mid = " << config.mid()
                         << "] has already existed.";
            return nullptr;
        }
        // Renegotiation is needed for the new or updated media track
        negotiation_needed_ = true;
        return std::dynamic_pointer_cast<AudioTrack>(media_track);
    });
}

std::shared_ptr<VideoTrack> PeerConnection::AddVideoTrack(const MediaTrack::Configuration& config) {
    return signaling_task_queue_->Invoke<std::shared_ptr<VideoTrack>>([this, &config]() -> std::shared_ptr<VideoTrack> {
        std::shared_ptr<MediaTrack> media_track = FindMediaTrack(config.mid());
        if (!media_track) {
            media_track = std::make_shared<VideoTrack>(config, &broadcaster_, worker_task_queue_.get());
            this->media_tracks_.emplace(std::make_pair(media_track->mid(), media_track));
        } else {
            PLOG_WARNING << "The media track ["
                         << "kind = " << config.kind()
                         << ", mid = " << config.mid()
                         << "] has already existed.";
            return nullptr;
        }
        // Renegotiation is needed for the new or updated media track
        negotiation_needed_ = true;
        return std::dynamic_pointer_cast<VideoTrack>(media_track);
    });
}

// Data Channels
std::shared_ptr<DataChannel> PeerConnection::AddDataChannel(const DataChannel::Init& init_config, std::optional<uint16_t> stream_id_opt) {
    return signaling_task_queue_->Invoke<std::shared_ptr<DataChannel>>([this, init_config, stream_id_opt=std::move(stream_id_opt)]() -> std::shared_ptr<DataChannel> {
        uint16_t stream_id;
        try {
            if (stream_id_opt.has_value()) {
                stream_id = stream_id_opt.value();
                if (stream_id > kMaxSctpStreamId) {
                    throw std::invalid_argument("Invalid DataChannel stream id.");
                }
            } else {
                // FRC 8832: The peer that initiates opening a data channel selects a stream identifier for 
                // which the corresponding incoming and outgoing streams are unused. If the side is acting as the DTLS client,
                // it MUST choose an even stream identifier, if the side is acting as the DTLS server, it MUST choose an odd one.
                // See https://tools.ietf.org/html/rfc8832#section-6
                // The stream id is not equvalent to the mid of application in SDP, which is only used to distinguish the data channel and DTLS role.
                stream_id = network_task_queue_->Invoke<int>([this](){
                    return (ice_transport_->role() == sdp::Role::ACTIVE) ? 0 : 1;
                });
                // Avoid conflict with existed data channel
                while (data_channels_.find(stream_id) != data_channels_.end()) {
                    if (stream_id >= kMaxSctpStreamId - 2) {
                        throw std::overflow_error("Too many DataChannels");
                    }
                    stream_id += 2;
                }
            }

            // We assume the DataChannel is not negotiacted
            auto data_channel = std::make_shared<DataChannel>(init_config, stream_id) ;
            data_channels_.emplace(stream_id, data_channel);

            this->data_channel_needed_ = true;
            // Renegotiation is needed if the curren local description does not have application
            if (!local_sdp_ || local_sdp_->HasApplication() == false) {
                this->negotiation_needed_ = true;
            }

            bool is_connected = network_task_queue_->Invoke<bool>([this](){
                // If sctp transport is connected yet, we open the data channel immidiately
                return sctp_transport_ && sctp_transport_->state() == SctpTransport::State::CONNECTED;
            });

            if (is_connected) {
                data_channel->Open(shared_from_this());
            }

            return data_channel;

        } catch (const std::exception& exp) {
            PLOG_ERROR << "Failed to add media track: " << exp.what();
            return nullptr;
        }
    });
}

} // namespace naivertc
