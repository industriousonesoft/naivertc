#include "rtc/call/rtp_demuxer.hpp"
#include "rtc/rtp_rtcp/common/rtp_utils.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet.hpp"

#include <plog/Log.h>

namespace naivertc {

bool RtpDemuxer::DeliverRtcpPacket(CopyOnWriteBuffer in_packet) const {
    if (!rtp::utils::IsRtpPacket(in_packet)) {
        return false;
    }

    return true;
}
    
} // namespace naivertc
