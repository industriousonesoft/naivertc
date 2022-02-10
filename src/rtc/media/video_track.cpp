#include "rtc/media/video_track.hpp"
#include "rtc/media/video_send_stream.hpp"

namespace naivertc {

VideoTrack::~VideoTrack() {}

void VideoTrack::Send(video::EncodedFrame encoded_frame) {
    worker_queue_->Async([this, encoded_frame=std::move(encoded_frame)](){
        auto send_stream = dynamic_cast<VideoSendStream*>(send_stream_.get());
        if (send_stream != nullptr) {
            send_stream->OnEncodedFrame(std::move(encoded_frame));
        }
    });
}
    
} // namespace naivertc
