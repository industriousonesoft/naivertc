#include "rtc/rtp_rtcp/rtp/fec/fec_test_helper.hpp"

namespace naivertc {
namespace test {
namespace {
    
constexpr int kPacketTimestampIncrement = 3000;

}  // namespace

RtpPacketGenerator::RtpPacketGenerator(uint32_t ssrc, uint8_t payload_type) 
    : num_packets_left_(0), 
      ssrc_(ssrc),
      payload_type_(payload_type),
      seq_num_(0), 
      timestamp_(0) {}

RtpPacketGenerator::~RtpPacketGenerator() {}

void RtpPacketGenerator::NewFrame(size_t num_packets) {
    num_packets_left_ = num_packets;
    timestamp_ += kPacketTimestampIncrement;
}

RtpPacket RtpPacketGenerator::NextRtpPacket(size_t payload_size, size_t padding_size) {
    RtpPacket rtp_packet;
    rtp_packet.Resize(kRtpHeaderSize + payload_size);

    rtp_packet.set_marker(num_packets_left_ == 1);
    rtp_packet.set_payload_type(payload_type_);
    rtp_packet.set_timestamp(timestamp_);
    rtp_packet.set_sequence_number(seq_num_);
    rtp_packet.set_ssrc(ssrc_);

    rtp_packet.SetPadding(padding_size);

    ++seq_num_;
    --num_packets_left_;

    return rtp_packet;
}

// UlpFecPacketGenerator
UlpFecPacketGenerator::UlpFecPacketGenerator(uint32_t ssrc,
                                             uint8_t media_payload_type, 
                                             uint8_t fec_payload_type,
                                             uint8_t red_payload_type) 
    : RtpPacketGenerator(ssrc, media_payload_type),
      fec_payload_type_(fec_payload_type),
      red_payload_type_(red_payload_type) {}

UlpFecPacketGenerator::~UlpFecPacketGenerator() {}

RtpPacketReceived UlpFecPacketGenerator::BuildMediaRedPacket(const RtpPacket& rtp_packet, bool is_recovered) {
    RtpPacketReceived red_packet;
    red_packet.CopyHeaderFrom(rtp_packet);

    auto rtp_payload = rtp_packet.payload();
    auto red_payload_data = red_packet.SetPayloadSize(rtp_payload.size() + 1/* 1 byte RED header */);
    // Add media payload type into RED header.
    red_payload_data[0] = rtp_packet.payload_type() & 0x7f;
    // Copy rest of payload/padding.
    memcpy(red_payload_data + 1, rtp_payload.data(), rtp_payload.size());

    // Set RED payload type.
    red_packet.set_payload_type(red_payload_type_);
    red_packet.set_is_recovered(is_recovered);

    return red_packet;
}

RtpPacketReceived UlpFecPacketGenerator::BuildUlpFecRedPacket(const CopyOnWriteBuffer& fec_packets) {
    // Create a fake media packet to get a correct RTP header.
    --num_packets_left_;
    RtpPacket rtp_packet = NextRtpPacket(0 /* payload size */, 0 /* padding size */);

    RtpPacketReceived red_packet;
    red_packet.CopyHeaderFrom(rtp_packet);
    red_packet.set_marker(false);

    uint8_t* red_payload_data = red_packet.SetPayloadSize(fec_packets.size() + 1 /* 1 byte RED header */);

    // Add FEC payload type into RED header.
    red_payload_data[0] = fec_payload_type_ & 0x7f;
    // Copy FEC payload data.
    memcpy(red_payload_data + 1, fec_packets.data(), fec_packets.size());

    // Set RED payload type.
    red_packet.set_payload_type(red_payload_type_);
    red_packet.set_is_recovered(false);

    return red_packet;

}

} // namespace test
} // namespace naivert 
