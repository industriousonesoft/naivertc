#ifndef _RTC_MEDIA_VIDEO_H264_RTP_PACKETIZER_H_
#define _RTC_MEDIA_VIDEO_H264_RTP_PACKETIZER_H_

#include "base/defines.hpp"
#include "rtc/media/video/h264/common.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_packetizer.hpp"

#include <vector>

namespace naivertc {

class RTC_CPP_EXPORT H264RtpPacketizer final: public RtpPacketizer {
public:
    H264RtpPacketizer(std::shared_ptr<RtpPacketizationConfig> rtp_config, 
                      PaylaodSizeLimits limits, 
                      H264::PacketizationMode packetization_mode = H264::PacketizationMode::NON_INTERLEAVED);
    ~H264RtpPacketizer();

    

private:
    H264::PacketizationMode packetization_mode_;
};
    
} // namespace naivertc


#endif