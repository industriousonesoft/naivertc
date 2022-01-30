#include "rtc/media/video_send_stream.hpp"

namespace naivertc {

VideoSendStream::VideoSendStream(Configuration config) 
    : rtp_video_sender_(std::make_unique<RtpVideoSender>(config.rtp, 
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
    if (config.rtp.flexfec.payload_type >= 0) {
        ssrcs_.push_back(config.rtp.flexfec.ssrc);
    }
}

VideoSendStream::~VideoSendStream() {
    RTC_RUN_ON(&sequence_checker_);
}

std::vector<uint32_t> VideoSendStream::ssrcs() const {
    RTC_RUN_ON(&sequence_checker_);
    return ssrcs_;
}

bool VideoSendStream::OnEncodedFrame(video::EncodedFrame encoded_frame) {
    RTC_RUN_ON(&sequence_checker_);
    return rtp_video_sender_->OnEncodedFrame(std::move(encoded_frame));
}

void VideoSendStream::OnRtcpPacket(CopyOnWriteBuffer in_packet) {
    RTC_RUN_ON(&sequence_checker_);
    rtp_video_sender_->OnRtcpPacket(std::move(in_packet));
}
    
} // namespace naivertc