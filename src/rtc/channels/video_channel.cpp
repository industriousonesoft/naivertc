#include "rtc/channels/video_channel.hpp"

namespace naivertc {

VideoChannel::VideoChannel(std::string mid, TaskQueue* worker_queue) 
    : MediaChannel(std::move(mid), worker_queue) {}

VideoChannel::~VideoChannel() {}

void VideoChannel::SetLocalMedia(sdp::Media media, sdp::Type type) {

}

void VideoChannel::SetRemoteMedia(sdp::Media media, sdp::Type type) {
    
}

} // namespace naivertc