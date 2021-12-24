#include "rtc/media/video_receive_stream.hpp"

namespace naivertc {

VideoReceiveStream::VideoReceiveStream(Configuration config, TaskQueue* task_queue) 
    : config_(std::move(config)),
      task_queue_(task_queue) {}

VideoReceiveStream::~VideoReceiveStream() {};

std::vector<uint32_t> VideoReceiveStream::ssrcs() const {
    return task_queue_->Sync<std::vector<uint32_t>>([this](){
        return ssrcs_;
    });
}

void VideoReceiveStream::OnRtpPacket(RtpPacketReceived in_packet) {}

void VideoReceiveStream::OnRtcpPacket(CopyOnWriteBuffer in_packet) {}

} // namespace naivertc