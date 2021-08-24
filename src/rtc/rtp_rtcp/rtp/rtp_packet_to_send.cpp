#include "rtc/rtp_rtcp/rtp/rtp_packet_to_send.hpp"

namespace naivertc {

RtpPacketToSend::RtpPacketToSend(size_t capacity)
    : RtpPacket(capacity) {}
RtpPacketToSend::RtpPacketToSend(const RtpPacketToSend& packet) = default;
RtpPacketToSend::RtpPacketToSend(RtpPacketToSend&& packet) = default;
RtpPacketToSend& RtpPacketToSend::operator=(const RtpPacketToSend& packet) = default;
RtpPacketToSend& RtpPacketToSend::operator=(RtpPacketToSend&& packet) = default;

RtpPacketToSend::RtpPacketToSend(std::shared_ptr<ExtensionManager> extension_manager) 
    : RtpPacket(extension_manager) {}

RtpPacketToSend::RtpPacketToSend(std::shared_ptr<ExtensionManager> extension_manager, size_t capacity) 
    : RtpPacket(extension_manager, capacity) {}

RtpPacketToSend::~RtpPacketToSend() = default;
    
} // namespace naivertc
