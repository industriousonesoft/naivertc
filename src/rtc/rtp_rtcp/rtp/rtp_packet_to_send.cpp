#include "rtc/rtp_rtcp/rtp/rtp_packet_to_send.hpp"

namespace naivertc {

RtpPacketToSend::RtpPacketToSend(size_t capacity)
    : RtpPacket(capacity) {}
RtpPacketToSend::RtpPacketToSend(const RtpPacketToSend& packet) = default;
RtpPacketToSend::RtpPacketToSend(RtpPacketToSend&& packet) = default;
RtpPacketToSend& RtpPacketToSend::operator=(const RtpPacketToSend& packet) = default;
RtpPacketToSend& RtpPacketToSend::operator=(RtpPacketToSend&& packet) = default;

RtpPacketToSend::~RtpPacketToSend() = default;
    
} // namespace naivertc
