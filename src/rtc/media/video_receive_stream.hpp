#ifndef _RTC_MEDIA_VIDEO_VIDEO_RECEIVE_STREAM_H_
#define _RTC_MEDIA_VIDEO_VIDEO_RECEIVE_STREAM_H_

#include "base/defines.hpp"
#include "rtc/api/media_receive_stream.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp_video_receiver.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.hpp"
#include "rtc/rtp_rtcp/components/rtp_receive_statistics.hpp"
#include "rtc/rtp_rtcp/rtp_video_receiver.hpp"

#include <map>

namespace naivertc {

class RTC_CPP_EXPORT VideoReceiveStream : public MediaReceiveStream,
                                          RtpVideoReceiver::CompleteFrameReceiver {
public:
    using RtpConfig = struct RtpVideoReceiver::Configuration;
    struct Configuration {
        Clock* clock;

        RtpConfig rtp;
    };  
public:
    VideoReceiveStream(Configuration config);
    ~VideoReceiveStream() override;

    std::vector<uint32_t> ssrcs() const override;

    // Implements RtpPacketSink
    void OnRtpPacket(RtpPacketReceived in_packet) override;
    // Implements RtcpPacketSink
    void OnRtcpPacket(CopyOnWriteBuffer in_packet) override;

    // Implements RtpVideoReceiver::CompleteFrameReceiver
    void OnCompleteFrame(rtp::video::FrameToDecode frame) override;

private:
    SequenceChecker sequence_checker_;
    Clock* const clock_;
    std::unique_ptr<TaskQueue> decode_queue_;
   
    std::vector<uint32_t> ssrcs_;

    std::unique_ptr<RtpReceiveStatistics> rtp_receive_stats_;

    std::unique_ptr<rtp::video::Timing> timing_;
    std::unique_ptr<rtp::video::jitter::FrameBuffer> frame_buffer_;

    RtpVideoReceiver rtp_video_receiver_;
};

} // namespace naivertc

#endif