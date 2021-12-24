#include "rtc/media/video_send_stream.hpp"

namespace naivertc {

VideoSendStream::VideoSendStream(Configuration config, TaskQueue* task_queue) 
    : task_queue_(task_queue),
      rtp_video_sender_(std::make_unique<RtpVideoSender>(config.rtp, 
                                                         config.clock, 
                                                         config.send_transport)) {
    // Media ssrc
    if (config.rtp.local_media_ssrc >= 0) {
        ssrcs_.push_back(config.rtp.local_media_ssrc);
    }
    // RTX ssrc
    if (config.rtp.rtx_send_ssrc) {
        ssrcs_.push_back(*config.rtp.rtx_send_ssrc);
    }
    // FLEX_FEC ssrc
    if (config.rtp.flexfec.ssrc >= 0) {
        ssrcs_.push_back(config.rtp.flexfec.ssrc);
    }
}

VideoSendStream::~VideoSendStream() {}

std::vector<uint32_t> VideoSendStream::ssrcs() const {
    return task_queue_->Sync<std::vector<uint32_t>>([this](){
        return ssrcs_;
    });
}

bool VideoSendStream::OnEncodedFrame(video::EncodedFrame encoded_frame) {
    return task_queue_->Sync<bool>([this, encoded_frame=std::move(encoded_frame)](){
        return rtp_video_sender_->OnEncodedFrame(std::move(encoded_frame));
    });
}

void VideoSendStream::OnRtcpPacket(CopyOnWriteBuffer in_packet) {

}
    
} // namespace naivertc