#include "rtc/rtp_rtcp/rtp/fec/fec_test_helper.hpp"

namespace naivertc {
namespace test {
namespace {

constexpr uint8_t kRtpMarkerBitMask = 0x80;

constexpr uint8_t kFecPayloadType = 96;
constexpr uint8_t kRedPayloadType = 97;
constexpr uint8_t kVp8PayloadType = 120;

constexpr int kPacketTimestampIncrement = 3000;

}  // namespace

RtpPacketGenerator::RtpPacketGenerator(uint32_t ssrc) 
    : num_packets_left_(0), 
      ssrc_(ssrc), 
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
    rtp_packet.set_payload_type(kVp8PayloadType);
    rtp_packet.set_timestamp(timestamp_);
    rtp_packet.set_sequence_number(seq_num_);
    rtp_packet.set_ssrc(ssrc_);

    rtp_packet.SetPadding(padding_size);

    ++seq_num_;
    --num_packets_left_;

    return rtp_packet;
}

// UlpFecPacketGenerator
UlpFecPacketGenerator::UlpFecPacketGenerator(uint32_t ssrc) 
    : RtpPacketGenerator(ssrc) {}

UlpFecPacketGenerator::~UlpFecPacketGenerator() {}

} // namespace test
} // namespace naivert 
