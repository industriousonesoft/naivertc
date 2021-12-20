#include "rtc/call/rtp_video_sender.hpp"

#include <plog/Log.h>

namespace naivertc {

RtpVideoSender::RtpVideoSender(const RtpRtcpConfig& rtp_rtcp_config,
                               video::CodecType codec_type,
                               std::shared_ptr<Clock> clock,
                               std::shared_ptr<Transport> send_transport,
                               std::shared_ptr<TaskQueue> task_queue) 
    : RtpMediaSender(rtp_rtcp_config, clock, send_transport, task_queue),
      sender_video_(codec_type, clock, rtp_sender_, task_queue) {
    
}

RtpVideoSender::~RtpVideoSender() {}

bool RtpVideoSender::OnEncodedFrame(video::EncodedFrame encoded_frame) {

    // rtp timestamp
    uint32_t rtp_timestamp = encoded_frame.timestamp();

    std::optional<int64_t> expected_restransmission_time_ms;
    if (encoded_frame.retransmission_allowed()) {
        expected_restransmission_time_ms = rtcp_module_->ExpectedRestransmissionTimeMs();
    }

    RtpVideoHeader video_header;
    video_header.frame_type = encoded_frame.frame_type();
    video_header.codec_type = encoded_frame.codec_type();
    video_header.frame_width = encoded_frame.width();
    video_header.frame_height = encoded_frame.height();

    bool bRet = sender_video_.Send(rtp_rtcp_config_.media_payload_type, 
                                   rtp_timestamp, 
                                   encoded_frame.capture_time_ms(),
                                   video_header,
                                   encoded_frame,
                                   expected_restransmission_time_ms);

    return bRet;
}

} // namespace naivertc
