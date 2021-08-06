#include "rtc/rtp_rtcp/rtp_packetizer.hpp"
#include "rtc/rtp_rtcp/rtp_packet.hpp"

namespace naivertc {

RtpPacketizer::RtpPacketizer(std::shared_ptr<RtpPacketizationConfig> rtp_config) : rtp_config_(rtp_config) {}

std::shared_ptr<BinaryBuffer> RtpPacketizer::Build(bool marker, std::shared_ptr<BinaryBuffer> payload) {
    auto rtp_packet = RtpPacket::Create(kRtpHeaderSize + payload->size());
    rtp_packet->set_marker(marker);
    rtp_packet->set_payload_type(rtp_config_->payload_type());
    rtp_packet->set_sequence_number(rtp_config_->sequence_num());
    rtp_packet->set_timestamp(rtp_config_->timestamp());
    rtp_packet->set_ssrc(rtp_config_->ssrc());
    rtp_packet->set_payload(payload->data(), payload->size());
    return rtp_packet;
}
    
} // namespace naivertc
