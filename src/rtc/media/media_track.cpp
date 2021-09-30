#include "rtc/media/media_track.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

MediaTrack::Kind MediaTackKindFromSDP(const sdp::Media& description) {
    switch(description.kind()) {
    case sdp::MediaEntry::Kind::AUDIO:
        return MediaTrack::Kind::AUDIO;
    case sdp::MediaEntry::Kind::VIDEO:
        return MediaTrack::Kind::VIDEO;
    default:
        return MediaTrack::Kind::UNKNOWN;
    }
}

} // namespace

// Media track
MediaTrack::MediaTrack(const Configuration& config) 
    : MediaChannel(config.kind(), config.mid()),
      local_description_(SDPBuilder::Build(config)) {}

MediaTrack::MediaTrack(sdp::Media description) 
    : MediaChannel(MediaTackKindFromSDP(description), description.mid()),
      local_description_(std::move(std::move(description))) {}

MediaTrack::~MediaTrack() {}

const sdp::Media* MediaTrack::local_description() const {
    return task_queue_.Sync<const sdp::Media*>([this](){
        return local_description_.has_value() ? &local_description_.value() : nullptr;
    });
}

bool MediaTrack::ReconfigLocalDescription(const Configuration& config) {
    return task_queue_.Sync<bool>([this, &config](){
        if (config.kind() != kind_) {
            PLOG_WARNING << "Failed to reconfig as the incomming kind=" << config.kind()
                         << " is different from media track kind=" << kind_;
            return false;
        }else if (config.mid() != mid_) {
            PLOG_WARNING << "Failed to reconfig as the incomming mid=" << config.mid()
                         << " is different from local media mid=" << mid_;
            return false;
        }
        local_description_ = SDPBuilder::Build(config);
        return local_description_.has_value();
    });
    
}

const sdp::Media* MediaTrack::remote_description() const {
    return task_queue_.Sync<const sdp::Media*>([this](){
        return remote_description_.has_value() ? &remote_description_.value() : nullptr;
    });
}

bool MediaTrack::OnRemoteDescription(sdp::Media description) {
    return task_queue_.Sync<bool>([this, remote_description=std::move(description)](){
        if (!local_description_.has_value()) {
            PLOG_WARNING << "Failed to set remote description before setting local description.";
            return false;
        }
        if (local_description_->kind() != remote_description.kind()) {
            PLOG_WARNING << "Failed to set remote description as remote media kind=" << remote_description.kind()
                         << " is different from local kind=" << local_description_->kind() ;
            return false;
        }else if (local_description_->mid() != remote_description.mid()) {
            PLOG_WARNING << "Failed to set remote description as remote media mid=" << remote_description.mid()
                         << " is different from local media mid=" << local_description_->mid();
            return false;
        }
        remote_description_.emplace(std::move(remote_description));
        return remote_description_.has_value();
    });
}

void MediaTrack::OnRtcpPacket(CopyOnWriteBuffer in_packet) {

}

void MediaTrack::OnRtpPacket(RtpPacketReceived in_packet) {

}

} // namespace naivertc