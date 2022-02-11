#ifndef _RTC_PC_BROADCASTER_H_
#define _RTC_PC_BROADCASTER_H_

#include "base/defines.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/rtp_rtcp/base/rtp_parameters.hpp"
#include "rtc/rtp_rtcp/components/rtp_demuxer.hpp"
#include "rtc/media/video/encoded_frame.hpp"

#include <unordered_map>

namespace naivertc {

class Clock;
class MediaTransport;
class VideoSendStream;

class RTC_CPP_EXPORT Broadcaster {
public:
    Broadcaster(Clock* clock, MediaTransport* send_transport);
    ~Broadcaster();

    void DeliverRtpPacket(CopyOnWriteBuffer in_packet, bool is_rtcp);

    void AddVideoSendStream(RtpParameters rtp_params);
    void AddVideoRecvStream(RtpParameters rtp_params);

    void Clear();

    void Send(video::EncodedFrame encoded_frame);

private:
    SequenceChecker worker_queue_checker_;
    Clock* const clock_;
    MediaTransport* send_transport_;

    RtpDemuxer rtp_demuxer_;

    std::unordered_map<uint32_t, std::unique_ptr<VideoSendStream>> video_send_streams_;
};
    
} // namespace naivertc


#endif