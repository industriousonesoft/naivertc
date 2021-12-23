#include "rtc/media/media_track.hpp"
#include "common/utils_random.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

MediaTrack::Kind ToKind(sdp::MediaEntry::Kind kind) {
    switch(kind) {
    case sdp::MediaEntry::Kind::AUDIO:
        return MediaTrack::Kind::AUDIO;
    case sdp::MediaEntry::Kind::VIDEO:
        return MediaTrack::Kind::VIDEO;
    default:
        RTC_NOTREACHED();
    }
}

} // namespace

// Media track
MediaTrack::MediaTrack(const Configuration& config) 
    : MediaChannel(config.kind(), config.mid()),
      local_description_(SdpBuilder::Build(config)),
      remote_description_(std::nullopt) {
}

MediaTrack::MediaTrack(sdp::Media remote_description) 
    : MediaChannel(ToKind(remote_description.kind()), remote_description.mid()),
      local_description_(std::nullopt),
      remote_description_(std::move(remote_description)) {}

MediaTrack::~MediaTrack() {}

const sdp::Media* MediaTrack::local_description() const {
    return task_queue_.Sync<const sdp::Media*>([this](){
        return local_description_ ? &local_description_.value() : nullptr;
    });
}

const sdp::Media* MediaTrack::remote_description() const {
    return task_queue_.Sync<const sdp::Media*>([this](){
        return remote_description_ ? &remote_description_.value() : nullptr;
    });
}

bool MediaTrack::Reconfig(const Configuration& config) {
    return task_queue_.Sync<bool>([this, &config](){
        if (config.kind() != kind_) {
            PLOG_WARNING << "Failed to reconfig as the incomming kind=" << config.kind()
                         << " is different from media track kind=" << kind_;
            return false;
        } else if (config.mid() != mid_) {
            PLOG_WARNING << "Failed to reconfig as the incomming mid=" << config.mid()
                         << " is different from local media mid=" << mid_;
            return false;
        }
        local_description_.emplace(SdpBuilder::Build(config));
        return true;
    });
}

bool MediaTrack::OnNegotiated(sdp::Media remote_description) {
    return task_queue_.Sync<bool>([this, remote_description=remote_description]{
        if (local_description_->kind() != remote_description.kind()) {
            PLOG_WARNING << "Failed to reconfig as the incomming kind=" << remote_description.kind()
                         << " is different from media track kind=" << kind_;
            return false;
        } else if (local_description_->mid() != remote_description.mid()) {
            PLOG_WARNING << "Failed to reconfig as the incomming mid=" << remote_description.mid()
                         << " is different from local media mid=" << mid_;
            return false;
        }
        remote_description_.emplace(std::move(remote_description));
        return true;
    });
}

} // namespace naivertc