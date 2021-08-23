#include "rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_packet.hpp"

namespace naivertc {

RtpPacketizer::RtpPacketizer(PayloadSizeLimits limits) 
    : limits_(limits) {}
    
} // namespace naivertc
