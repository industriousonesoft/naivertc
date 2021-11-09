#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_JITTER_DEFINES_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_JITTER_DEFINES_H_

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {

enum class ProtectionMode {
    NACK,
    NACK_FEC
};

} // namespace jitter
} // namespace video
} // namespace rtp 
} // namespace naivert 

#endif