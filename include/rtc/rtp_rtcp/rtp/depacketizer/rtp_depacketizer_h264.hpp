#ifndef _RTC_RTP_RTCP_RTP_DEPACKETIZER_RTP_DEPACKETIZER_H264_H_
#define _RTC_RTP_RTCP_RTP_DEPACKETIZER_RTP_DEPACKETIZER_H264_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer.hpp"

namespace naivertc {

class CopyOnWriteBuffer;

// This class is not thread-safety, the caller MUST provide that.
class RTC_CPP_EXPORT RtpH264Depacketizer : public RtpDepacketizer {
public:
    ~RtpH264Depacketizer() override = default;

    std::optional<Packet> Depacketize(CopyOnWriteBuffer rtp_payload) override;

private:
    std::optional<Packet> DepacketizeStapAOrSingleNalu(CopyOnWriteBuffer rtp_payload);
    std::optional<Packet> DepacketizeFuANalu(CopyOnWriteBuffer rtp_payload);
};

} // namespace naivertc

#endif