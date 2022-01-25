#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

namespace naivertc {

RtpPacketReceived::RtpPacketReceived() = default;
RtpPacketReceived::RtpPacketReceived(const HeaderExtensionMap* extension_map, Timestamp arrival_time) 
    : RtpPacket(extension_map),
      arrival_time_(arrival_time) {}
RtpPacketReceived::RtpPacketReceived(const RtpPacketReceived& other) = default;
RtpPacketReceived::RtpPacketReceived(RtpPacketReceived&& other) = default;

RtpPacketReceived& RtpPacketReceived::operator=(const RtpPacketReceived& other) = default;
RtpPacketReceived& RtpPacketReceived::operator=(RtpPacketReceived&& other) = default;

RtpPacketReceived::~RtpPacketReceived() {}
    
} // namespace naivertc
