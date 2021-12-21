#ifndef _RTC_CALL_VIDEO_SENDER_H_
#define _RTC_CALL_VIDEO_SENDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_sender_video.hpp"
#include "rtc/media/video/encoded_frame.hpp"
#include "rtc/call/rtp_media_sender.hpp"
#include "rtc/media/video/common.hpp"

#include <vector>

namespace naivertc {

// RtpVideoSender
class RTC_CPP_EXPORT RtpVideoSender : public RtpMediaSender {
public:
    RtpVideoSender(RtpRtcpConfig rtp_rtcp_config,
                   video::CodecType codec_type,
                   Clock* clock,
                   Transport* send_transport);
    ~RtpVideoSender();

    MediaType media_type() const override { return MediaType::VIDEO; }

    bool OnEncodedFrame(video::EncodedFrame encoded_frame);

private:
    RtpSenderVideo sender_video_;
};

} // namespace naivertc


#endif