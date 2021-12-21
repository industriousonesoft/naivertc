#ifndef _RTC_CALL_VIDEO_SEND_STREAM_H_
#define _RTC_CALL_VIDEO_SEND_STREAM_H_

#include "base/defines.hpp"
#include "rtc/call/rtp_video_sender.hpp"

namespace naivertc {

class RTC_CPP_EXPORT VideoSendStream {
public:
    struct Configuration {
        using RtpConfig = struct RtpVideoSender::Configuration;
        RtpConfig config;
    };
public:
    VideoSendStream(Configuration config);
    ~VideoSendStream();
};
    
} // namespace naivertc


#endif