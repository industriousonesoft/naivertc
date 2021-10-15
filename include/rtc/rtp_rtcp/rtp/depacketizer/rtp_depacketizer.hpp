#ifndef _RTC_RTP_RTCP_DEPACKETIZER_RTP_DEPACKETIZER_H_
#define _RTC_RTP_RTCP_DEPACKETIZER_RTP_DEPACKETIZER_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp_video_header.hpp"
#include "rtc/media/video/codecs/h264/common.hpp"

#include <variant>
#include <optional>

namespace naivertc {

using RtpVideoCodecPacketizationInfo = std::variant<h264::PacketizationInfo>;

class RTC_CPP_EXPORT RtpDepacketizer {
public:
    struct DepacketizedPayload {
        RtpVideoHeader video_header;
        CopyOnWriteBuffer video_payload;
        RtpVideoCodecPacketizationInfo packetization_info;
    };
public:
    virtual ~RtpDepacketizer() = default;
    virtual std::optional<DepacketizedPayload> Depacketize(CopyOnWriteBuffer rtp_payload) = 0;
};
    
} // namespace naivertc


#endif