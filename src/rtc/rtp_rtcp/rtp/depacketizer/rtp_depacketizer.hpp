#ifndef _RTC_RTP_RTCP_DEPACKETIZER_RTP_DEPACKETIZER_H_
#define _RTC_RTP_RTCP_DEPACKETIZER_RTP_DEPACKETIZER_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp_video_header.hpp"
#include "rtc/media/video/codecs/h264/common.hpp"

#include <variant>
#include <optional>

namespace naivertc {

using RtpVideoCodecHeader = std::variant<std::monostate,
                                         h264::PacketizationInfo>;

class RtpDepacketizer {
public:
    struct Packet {
        RtpVideoHeader video_header;
        RtpVideoCodecHeader video_codec_header;
        CopyOnWriteBuffer video_payload;
    };
public:
    virtual ~RtpDepacketizer() = default;
    virtual std::optional<Packet> Depacketize(CopyOnWriteBuffer rtp_payload) = 0;
};
    
} // namespace naivertc


#endif