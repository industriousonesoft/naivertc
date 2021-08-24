#ifndef _RTC_RTP_RTCP_RTP_VIDEO_HEADER_H_
#define _RTC_RTP_RTCP_RTP_VIDEO_HEADER_H_

#include "base/defines.hpp"
#include "rtc/media/video/common.hpp"

namespace naivertc {

struct RTC_CPP_EXPORT RtpVideoHeader { 
public:
    RtpVideoHeader();
    RtpVideoHeader(const RtpVideoHeader& other);
    ~RtpVideoHeader();

    uint16_t frame_width = 0;
    uint16_t frame_height = 0;
    
    video::FrameType frame_type = video::FrameType::EMPTTY;
    video::CodecType codec_type = video::CodecType::NONE;

};
    
} // namespace naivertc


#endif