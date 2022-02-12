#include "rtc/media/video_track.hpp"
#include "rtc/pc/broadcaster.hpp"

namespace naivertc {

VideoTrack::~VideoTrack() {}

void VideoTrack::Send(video::EncodedFrame encoded_frame) {
    worker_queue_->Post([this, encoded_frame=std::move(encoded_frame)](){
        if (broadcaster_) {
            broadcaster_->Send(std::move(encoded_frame));
        }
    });
}
    
} // namespace naivertc
