#ifndef _RTC_CALL_VIDEO_SENDER_H_
#define _RTC_CALL_VIDEO_SENDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_impl.hpp"
#include "rtc/rtp_rtcp/rtp/video/rtp_video_sender.hpp"
#include "rtc/transports/transport.hpp"

namespace naivertc {

class RTC_CPP_EXPORT VideoSender {
public:
    // Sender for a single simulcast stream
    struct StreamSender {
        StreamSender(std::unique_ptr<RtpRtcpImpl> rtp_rtcp, 
                     std::unique_ptr<RtpVideoSender> rtp_video_sender);
        StreamSender(StreamSender&&) = default;
        StreamSender& operator=(StreamSender&&) = default;
        ~StreamSender();

    private:
        friend class VideoSender;
        std::unique_ptr<RtpRtcpImpl> rtp_rtcp;
        std::unique_ptr<RtpVideoSender> rtp_video_sender;
    };

public:
    VideoSender(std::shared_ptr<Clock> clock, 
                std::shared_ptr<Transport> send_transport);
    ~VideoSender();

private:
    std::shared_ptr<Clock> clock_;
};
    
} // namespace naivertc


#endif