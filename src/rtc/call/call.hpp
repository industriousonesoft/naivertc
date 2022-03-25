#ifndef _RTC_CALL_CALL_H_
#define _RTC_CALL_CALL_H_

#include "base/defines.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/rtp_rtcp/base/rtp_parameters.hpp"
#include "rtc/rtp_rtcp/components/rtp_demuxer.hpp"
#include "rtc/media/video/encoded_frame.hpp"

#include <unordered_map>
#include <set>

namespace naivertc {

class Clock;
class RtcMediaTransport;
class VideoSendStream;
class VideoReceiveStream;
class MediaReceiveStream;
class RtpSendController;

class Call {
public:
    Call(Clock* clock, RtcMediaTransport* send_transport);
    ~Call();

    void AddVideoSendStream(const RtpParameters& rtp_params);
    void AddVideoRecvStream(const RtpParameters& rtp_params);

    void Clear();

    void Send(video::EncodedFrame encoded_frame);

    void DeliverRtpPacket(CopyOnWriteBuffer in_packet, bool is_rtcp);

private:
    SequenceChecker worker_queue_checker_;
    Clock* const clock_;
    RtcMediaTransport* send_transport_;

    std::set<std::unique_ptr<VideoSendStream>> video_send_streams_;
    std::set<std::unique_ptr<VideoReceiveStream>> video_recv_streams_;

    std::unordered_map<uint32_t, MediaReceiveStream*> recv_streams_by_ssrc_;

    RtpDemuxer rtp_demuxer_;
    std::unique_ptr<RtpSendController> send_controller_;
    
};
    
} // namespace naivertc


#endif