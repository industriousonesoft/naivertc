#include "rtc/call/video_send_stream.hpp"

namespace naivertc {

VideoSendStream::VideoSendStream(const Configuration& config) 
    : rtp_video_sender_(config.rtp, config.clock, config.send_transport) {}

VideoSendStream::~VideoSendStream() {}

bool OnEncodedFrame(video::EncodedFrame encoded_frame) {
    RTC_RUN_ON(&sequence_checker_);
    return rtp_video_sender_->OnEncodedFrame(std::move(encoded_frame));
}
    
} // namespace naivertc