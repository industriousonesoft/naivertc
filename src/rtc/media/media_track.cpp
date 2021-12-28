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

std::string ToString(MediaTrack::Kind kind) {
    switch(kind) {
    case MediaTrack::Kind::AUDIO:
        return "audio";
    case MediaTrack::Kind::VIDEO:
        return "video";
    default:
        RTC_NOTREACHED();
    }
}

} // namespace

// Media track
MediaTrack::MediaTrack(const Configuration& config) 
    : MediaTrack(SdpBuilder::Build(config)) {}

MediaTrack::MediaTrack(sdp::Media description)
    : kind_(ToKind(description.kind())),
      description_(std::move(description)),
      task_queue_(std::make_unique<TaskQueue>(ToString(kind_) + "mediatrack" + description_.mid() + "task.queue")) {}

MediaTrack::~MediaTrack() {}

MediaTrack::Kind MediaTrack::kind() const {
    return kind_;
}

const std::string MediaTrack::mid() const {
    return description_.mid();
}

sdp::Media MediaTrack::description() const {
    return description_;
}

bool MediaTrack::is_opened() const {
    return task_queue_->Sync<bool>([this](){
        return is_opened_;
    });
}

void MediaTrack::OnOpened(OpenedCallback callback) {
    task_queue_->Async([this, callback=std::move(callback)](){
        opened_callback_ = callback;
        TriggerOpen();
    });
}

void MediaTrack::OnClosed(ClosedCallback callback) {
    task_queue_->Async([this, callback=std::move(callback)](){
        closed_callback_ = callback;
        TriggerClose();
    });
}

void MediaTrack::TriggerOpen() {
    task_queue_->Async([this](){
        if (is_opened_) {
            return;
        }
        is_opened_ = true;
        if (opened_callback_) {
            opened_callback_();
        }
    });
}

void MediaTrack::TriggerClose() {
    task_queue_->Async([this](){
        if (!is_opened_) {
            return;
        }
        is_opened_ = false;
        if (closed_callback_) {
            closed_callback_();
        }
    });
}

} // namespace naivertc