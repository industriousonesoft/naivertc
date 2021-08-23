#include "rtc/rtp_rtcp/rtp/rtp_video_sender.hpp"

namespace naivertc {

RtpVideoSender::RtpVideoSender(const Configuration& config) 
    : clock_(config.clock) {
}
    
RtpVideoSender::~RtpVideoSender() {

}
    
} // namespace naivertc
