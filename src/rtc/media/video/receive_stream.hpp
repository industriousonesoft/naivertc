#ifndef _RTC_MEDIA_VIDEO_RECEIVE_STREAM_H_
#define _RTC_MEDIA_VIDEO_RECEIVE_STREAM_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/call/rtp_video_receiver.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/api/rtp_packet_sink.hpp"

#include <map>

namespace naivertc {

class RTC_CPP_EXPORT VideoReceiveStream : public RtpPacketSink {
public:
    struct Configuration {
        using RtpConfig = struct RtpVideoReceiver::Configuration;
        RtpConfig rtp;
    };  
public:
    VideoReceiveStream(Configuration config);
    ~VideoReceiveStream();

    // RtpPacketSink interfaces
    void OnRtpPacket(RtpPacketReceived in_packet) override;
    void OnRtcpPacket(CopyOnWriteBuffer in_packet) override;

private:
    SequenceChecker sequence_checker_;
    const Configuration config_;
};

} // namespace naivertc

#endif