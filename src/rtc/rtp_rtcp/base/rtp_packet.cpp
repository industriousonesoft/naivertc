#include "rtc/rtp_rtcp/base/rtp_packet.hpp"

namespace naivertc {

RtpPacket::RtpPacket(std::shared_ptr<Packet> raw_packet, Type type, SSRCId ssrc_id) 
    : raw_packet_(std::move(raw_packet)),
    type_(type),
    ssrc_id_(ssrc_id) {}
    
RtpPacket::~RtpPacket() {
    raw_packet_.reset();
}

} // namespace naivertc
