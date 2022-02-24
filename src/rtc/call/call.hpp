#ifndef _RTC_CALL_CALL_H_
#define _RTC_CALL_CALL_H_

#include "base/defines.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/rtp_rtcp/base/rtp_parameters.hpp"
#include "rtc/rtp_rtcp/components/rtp_demuxer.hpp"
#include "rtc/media/video/encoded_frame.hpp"
#include "rtc/call/rtp_send_controller.hpp"

#include <unordered_map>
#include <set>

namespace naivertc {

class Clock;
class RtcMediaTransport;
class VideoSendStream;

class Call {
public:
    Call(Clock* clock, RtcMediaTransport* send_transport);
    ~Call();

    void AddVideoSendStream(RtpParameters rtp_params);
    void AddVideoRecvStream(RtpParameters rtp_params);

    void Clear();

    void Send(video::EncodedFrame encoded_frame);

    void DeliverRtpPacket(CopyOnWriteBuffer in_packet, bool is_rtcp);

private:
    SequenceChecker worker_queue_checker_;
    Clock* const clock_;
    RtcMediaTransport* send_transport_;

    std::unordered_map<uint32_t, rtp::HeaderExtensionMap> recv_rtp_ext_maps_;
    std::set<std::unique_ptr<VideoSendStream>> video_send_streams_;

    RtpDemuxer rtp_demuxer_;
    RtpSendController send_controller_;
    
};
    
} // namespace naivertc


#endif