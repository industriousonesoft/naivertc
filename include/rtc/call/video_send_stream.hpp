#ifndef _RTC_CALL_VIDEO_SENDER_H_
#define _RTC_CALL_VIDEO_SENDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_impl.hpp"
#include "rtc/rtp_rtcp/rtp/video/rtp_video_sender.hpp"
#include "rtc/media/video/encoded_frame.hpp"
#include "rtc/call/rtp_config.hpp"

#include <vector>

namespace naivertc {

// VideoSendStream
class RTC_CPP_EXPORT VideoSendStream {
public:
    VideoSendStream(const RtpConfig& rtp_config,
                    std::shared_ptr<Clock> clock,
                    std::shared_ptr<Transport> send_transport, 
                    std::shared_ptr<TaskQueue> task_queue);
    ~VideoSendStream();

    bool SendEncodedFrame(std::shared_ptr<VideoEncodedFrame> encoded_frame);

private:
    std::unique_ptr<RtpVideoSender> rtp_video_sender;
};

} // namespace naivertc


#endif