#include "rtc/rtp_rtcp/rtp/fec/flex/fec_generator_flex.hpp"

namespace naivertc {

FlexfecGenerator::FlexfecGenerator(int payload_type,
                                   uint32_t ssrc,
                                   uint32_t protected_media_ssrc) 
    : payload_type_(payload_type),
      ssrc_(ssrc),
      protected_media_ssrc_(protected_media_ssrc) {}

FlexfecGenerator::~FlexfecGenerator() = default;

size_t FlexfecGenerator::MaxPacketOverhead() const {
    return 0;
}

void FlexfecGenerator::SetProtectionParameters(const FecProtectionParams& delta_params, 
                                               const FecProtectionParams& key_params) {}

void FlexfecGenerator::PushMediaPacket(RtpPacketToSend packet) {}

std::vector<RtpPacketToSend> FlexfecGenerator::PopFecPackets() {
    std::vector<RtpPacketToSend> fec_packets; 
    return fec_packets;
}
    
} // namespace naivertc
