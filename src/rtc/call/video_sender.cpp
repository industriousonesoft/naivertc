#include "rtc/call/video_sender.hpp"

namespace naivertc {

// StreamSender implemention
VideoSender::StreamSender::StreamSender(std::unique_ptr<RtpRtcpImpl> rtp_rtcp, 
                                        std::unique_ptr<RtpVideoSender> rtp_video_sender) 
    : rtp_rtcp(std::move(rtp_rtcp)),
      rtp_video_sender(std::move(rtp_video_sender)) {}

VideoSender::StreamSender::~StreamSender() = default;

// VideoSender implemention
VideoSender::VideoSender(std::shared_ptr<Clock> clock, 
                         std::shared_ptr<Transport> send_transport) 
    : clock_(clock) {

}

VideoSender::~VideoSender() {

}

} // namespace naivertc
