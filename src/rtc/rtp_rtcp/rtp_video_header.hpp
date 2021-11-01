#ifndef _RTC_RTP_RTCP_RTP_VIDEO_HEADER_H_
#define _RTC_RTP_RTCP_RTP_VIDEO_HEADER_H_

#include "base/defines.hpp"
#include "rtc/media/video/common.hpp"

namespace naivertc {

// TODO: Put into encoded frame
struct RTC_CPP_EXPORT RtpVideoHeader {
public:
    RtpVideoHeader() = default;
    RtpVideoHeader(const RtpVideoHeader& other) = default;
    ~RtpVideoHeader() = default;

    uint16_t frame_width = 0;
    uint16_t frame_height = 0;
    
    VideoFrameType frame_type = VideoFrameType::EMPTTY;
    VideoCodecType codec_type = VideoCodecType::GENERIC;

    VideoPlayoutDelay playout_delay;

    bool is_first_packet_in_frame = false;
    bool is_last_packet_in_frame = false;
};
    
} // namespace naivertc


#endif