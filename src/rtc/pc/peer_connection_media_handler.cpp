#include "rtc/pc/peer_connection.hpp"
#include "rtc/transports/sctp_transport_internals.hpp"

#include <plog/Log.h>

namespace naivertc {
    
std::shared_ptr<MediaTrack> PeerConnection::AddTrack(const MediaTrack::Configuration& config) {
    return signal_task_queue_->Sync<std::shared_ptr<MediaTrack>>([this, &config]() -> std::shared_ptr<MediaTrack> {
        if (config.kind() != MediaTrack::Kind::UNKNOWN) {
            std::shared_ptr<MediaTrack> media_track = FindMediaTrack(config.mid());
            if (!media_track) {
                media_track = std::make_shared<MediaTrack>(config);
                this->media_tracks_.emplace(std::make_pair(media_track->mid(), media_track));
            }else {
                media_track->ReconfigLocalDescription(config);
            }
            // Renegotiation is needed for the new or updated media track
            negotiation_needed_ = true;
            return media_track;
        }else {
            PLOG_WARNING << "Failed to add a unknown kind media track.";
            return nullptr;
        }
    });
}

// Data Channels
std::shared_ptr<DataChannel> PeerConnection::CreateDataChannel(const DataChannel::Init& init_config, std::optional<uint16_t> stream_id_opt) {
    return signal_task_queue_->Sync<std::shared_ptr<DataChannel>>([this, init_config, stream_id_opt=std::move(stream_id_opt)]() -> std::shared_ptr<DataChannel> {
        uint16_t stream_id;
        try {
            if (stream_id_opt.has_value()) {
                stream_id = stream_id_opt.value();
                if (stream_id > kMaxSctpStreamId) {
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
                    if (stream_id >= kMaxSctpStreamId - 2) {
                        throw std::overflow_error("Too many DataChannels");
                    }
                    stream_id += 2;
                }
            }

            // We assume the DataChannel is not negotiacted
            auto data_channel = std::make_shared<DataChannel>(init_config, stream_id) ;

            // If sctp transport is connected yet, we open the data channel immidiately
            if (sctp_transport_ && sctp_transport_->state() == SctpTransport::State::CONNECTED) {
                data_channel->Open(sctp_transport_);
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
