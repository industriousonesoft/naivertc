#ifndef _RTC_MEDIA_VIDEO_SEND_STREAM_H_
#define _RTC_MEDIA_VIDEO_SEND_STREAM_H_

#include "base/defines.hpp"
#include "rtc/call/rtp_video_sender.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/api/media_transport.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/api/video_encoded_frame_sink.hpp"

#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT VideoSendStream : public VideoEncodedFrameSink {
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

    bool OnEncodedFrame(video::EncodedFrame encoded_frame) override;

private:
    SequenceChecker sequence_checker_;
    std::unique_ptr<RtpVideoSender> rtp_video_sender_;
};
    
} // namespace naivertc


#endif