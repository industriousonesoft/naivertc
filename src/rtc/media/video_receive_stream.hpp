#ifndef _RTC_MEDIA_VIDEO_VIDEO_RECEIVE_STREAM_H_
#define _RTC_MEDIA_VIDEO_VIDEO_RECEIVE_STREAM_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/call/rtp_video_receiver.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/api/rtp_packet_sink.hpp"

#include <map>

namespace naivertc {

class RTC_CPP_EXPORT VideoReceiveStream : public RtpPacketSink,
                                          public RtcpPacketSink {
public:
    struct Configuration {
        using RtpConfig = struct RtpVideoReceiver::Configuration;
        RtpConfig rtp;
    };  
public:
    VideoReceiveStream(Configuration config, TaskQueue* task_queue);
    ~VideoReceiveStream();

    std::vector<uint32_t> ssrcs() const;

    // RtpPacketSink interfaces
    void OnRtpPacket(RtpPacketReceived in_packet) override;
    // RtcpPacketSink interfaces
    void OnRtcpPacket(CopyOnWriteBuffer in_packet) override;

private:
    SequenceChecker sequence_checker_;
    const Configuration config_;
    TaskQueue* const task_queue_;

    std::vector<uint32_t> ssrcs_;
};

} // namespace naivertc

#endif