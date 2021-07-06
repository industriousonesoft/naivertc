#include "pc/rtp_rtcp/rtp_packet.hpp"

namespace naivertc {

RtpPacket::RtpPacket(std::shared_ptr<Packet> raw_packet, Type type, SSRCId ssrc_id) 
    : raw_packet_(std::move(raw_packet)),
    type_(type),
    ssrc_id_(ssrc_id) {}
    
RtpPacket::~RtpPacket() {
    raw_packet_.reset();
}

 bool RtpPacket::is_empty() const {
    if (raw_packet_) {
        raw_packet_->is_empty();
    }
    return false;
}

const char* RtpPacket::data() const {
    return raw_packet_->data();
}

char* RtpPacket::data() {
    return raw_packet_->data();
}

size_t RtpPacket::size() const {
    return raw_packet_->size();
}

const std::vector<std::byte> RtpPacket::bytes() const {
    return raw_packet_->bytes();
}

} // namespace naivertc
