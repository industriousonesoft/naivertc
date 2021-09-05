#include "rtc/call/video_send_stream.hpp"

#include <plog/Log.h>

namespace naivertc {

VideoSendStream::VideoSendStream(const RtpConfig& rtp_config,
                                 std::shared_ptr<Clock> clock,
                                 std::shared_ptr<Transport> send_transport,
                                 std::shared_ptr<TaskQueue> task_queue) {
}

VideoSendStream::~VideoSendStream() {

}

bool VideoSendStream::SendEncodedFrame(std::shared_ptr<VideoEncodedFrame> encoded_frame) {
    
    return true;
}

} // namespace naivertc
