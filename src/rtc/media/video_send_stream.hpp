#ifndef _RTC_MEDIA_VIDEO_VIDEO_SEND_STREAM_H_
#define _RTC_MEDIA_VIDEO_VIDEO_SEND_STREAM_H_

#include "base/defines.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/transports/rtc_transport_media.hpp"
#include "rtc/media/video/encoded_frame_sink.hpp"
#include "rtc/media/media_send_stream.hpp"
#include "rtc/rtp_rtcp/rtp_video_sender.hpp"

#include <memory>

namespace naivertc {

class VideoSendStream : public VideoEncodedFrameSink,
                        public MediaSendStream {
public:
    using Configuration = RtpVideoSender::Configuration;
public:
    VideoSendStream(const Configuration& config);
    ~VideoSendStream() override;

    std::vector<uint32_t> ssrcs() const override;

    // VideoEncodedFrameSink interfaces
    bool OnEncodedFrame(video::EncodedFrame encoded_frame) override;

    // RtcpPacketSink interfaces
    void OnRtcpPacket(CopyOnWriteBuffer in_packet) override;

private:
    SequenceChecker sequence_checker_;
    std::unique_ptr<RtpVideoSender> rtp_video_sender_;

    std::vector<uint32_t> ssrcs_;
};
    
} // namespace naivertc


#endif