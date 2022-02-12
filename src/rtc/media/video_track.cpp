#include "rtc/media/video_track.hpp"
#include "rtc/call/call.hpp"

namespace naivertc {

VideoTrack::~VideoTrack() {}

void VideoTrack::Send(video::EncodedFrame encoded_frame) {
    worker_queue_->Post([this, encoded_frame=std::move(encoded_frame)](){
        if (call_) {
            call_->Send(std::move(encoded_frame));
        }
    });
}
    
} // namespace naivertc
