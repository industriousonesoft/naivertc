#include "rtc/call/rtp_video_sender.hpp"

#include <plog/Log.h>

namespace naivertc {

RtpVideoSender::RtpVideoSender(RtpRtcpConfig rtp_rtcp_config,
                               video::CodecType codec_type,
                               Clock* clock,
                               Transport* send_transport) 
    : RtpMediaSender(std::move(rtp_rtcp_config), clock, send_transport),
      sender_video_(codec_type, clock, rtp_sender_.get()) {}

RtpVideoSender::~RtpVideoSender() {}

bool RtpVideoSender::OnEncodedFrame(video::EncodedFrame encoded_frame) {
    RTC_RUN_ON(&sequence_checker_);
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
