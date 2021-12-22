#ifndef _RTC_CALL_VIDEO_SEND_STREAM_H_
#define _RTC_CALL_VIDEO_SEND_STREAM_H_

#include "base/defines.hpp"
#include "rtc/call/rtp_video_sender.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/channels/media_transport_interface.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT VideoSendStream {
public:
    struct Configuration {
        using RtpConfig = struct RtpVideoSender::Configuration;
        RtpConfig rtp;

        Clock* clock;
        MediaTransport* send_transport = nullptr;
    };
public:
    VideoSendStream(const Configuration& config);
    ~VideoSendStream();

    bool OnEncodedFrame(video::EncodedFrame encoded_frame);

private:
    SequenceChecker sequence_checker_;
    std::unique_ptr<RtpVideoSender> rtp_video_sender_;
};
    
} // namespace naivertc


#endif