#ifndef _RTC_MEDIA_VIDEO_VIDEO_SEND_STREAM_H_
#define _RTC_MEDIA_VIDEO_VIDEO_SEND_STREAM_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp_video_sender.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/api/media_transport.hpp"
#include "rtc/api/video_encoded_frame_sink.hpp"
#include "rtc/api/media_send_stream.hpp"

#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT VideoSendStream : public VideoEncodedFrameSink,
                                       public MediaSendStream {
public:
    struct Configuration {
        using RtpConfig = struct RtpVideoSender::Configuration;
        RtpConfig rtp;

        Clock* clock;
        MediaTransport* send_transport = nullptr;
    };
public:
    VideoSendStream(Configuration config, TaskQueue* task_queue);
    ~VideoSendStream() override;

    std::vector<uint32_t> ssrcs() const override;

    // VideoEncodedFrameSink interfaces
    bool OnEncodedFrame(video::EncodedFrame encoded_frame) override;

    // RtcpPacketSink interfaces
    void OnRtcpPacket(CopyOnWriteBuffer in_packet) override;

private:
    TaskQueue* const task_queue_;
    std::unique_ptr<RtpVideoSender> rtp_video_sender_;

    std::vector<uint32_t> ssrcs_;
};
    
} // namespace naivertc


#endif