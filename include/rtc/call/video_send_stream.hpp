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
    // RtpStream: Sender for a single simulcast stream
    struct RtpStream {
        RtpStream(std::unique_ptr<RtpRtcpImpl> rtp_rtcp, 
                     std::unique_ptr<RtpVideoSender> rtp_video_sender);
        RtpStream(RtpStream&&) = default;
        RtpStream& operator=(RtpStream&&) = default;
        ~RtpStream();
    private:
        friend class VideoSendStream;
        std::unique_ptr<RtpRtcpImpl> rtp_rtcp;
        std::unique_ptr<RtpVideoSender> rtp_video_sender;
        std::shared_ptr<FecGenerator> fec_generator;
    };
public:
    VideoSendStream(std::shared_ptr<Clock> clock, 
                    const RtpConfig& rtp_config,
                    std::shared_ptr<Transport> send_transport, 
                    std::shared_ptr<TaskQueue> task_queue);
    ~VideoSendStream();

    bool SendEncodedFrame(std::shared_ptr<VideoEncodedFrame> encoded_frame);

private:
    void InitRtpStreams(std::shared_ptr<Clock> clock, 
                        const RtpConfig& rtp_config,
                        std::shared_ptr<Transport> send_transport,
                        std::shared_ptr<TaskQueue> task_queue);
    std::shared_ptr<FecGenerator> CreateFecGeneratorIfNecessary(const RtpConfig& rtp_config, uint32_t media_ssrc);

private:
    const RtpConfig rtp_config_;
    std::shared_ptr<TaskQueue> task_queue_;
    std::vector<RtpStream> rtp_streams_;
};

} // namespace naivertc


#endif