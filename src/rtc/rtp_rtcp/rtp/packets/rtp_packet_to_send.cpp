#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"

namespace naivertc {

RtpPacketToSend::RtpPacketToSend(size_t capacity)
    : RtpPacket(capacity) {}
RtpPacketToSend::RtpPacketToSend(const RtpPacketToSend& packet) = default;
RtpPacketToSend::RtpPacketToSend(RtpPacketToSend&& packet) = default;
RtpPacketToSend& RtpPacketToSend::operator=(const RtpPacketToSend& packet) = default;
RtpPacketToSend& RtpPacketToSend::operator=(RtpPacketToSend&& packet) = default;

RtpPacketToSend::RtpPacketToSend(const HeaderExtensionMap* extension_map) 
    : RtpPacket(extension_map) {}

RtpPacketToSend::RtpPacketToSend(const HeaderExtensionMap* extension_map, size_t capacity) 
    : RtpPacket(extension_map, capacity) {}

RtpPacketToSend::~RtpPacketToSend() = default;
    
} // namespace naivertc
