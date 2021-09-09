#include "rtc/call/rtp_video_sender.hpp"

#include <plog/Log.h>

namespace naivertc {

RtpVideoSender::RtpVideoSender(const RtpRtcpConfig& rtp_rtcp_config,
                               video::CodecType codec_type,
                               std::shared_ptr<Clock> clock,
                               std::shared_ptr<Transport> send_transport,
                               std::shared_ptr<TaskQueue> task_queue) 
    : RtpMediaSender(rtp_rtcp_config, clock, send_transport, task_queue),
      rtp_video_sender_(codec_type, clock, rtp_sender_, task_queue) {
    
}

RtpVideoSender::~RtpVideoSender() {

}

bool RtpVideoSender::SendEncodedFrame(std::shared_ptr<VideoEncodedFrame> encoded_frame) {
    // TODO: Implements this.
    return true;
}

} // namespace naivertc
