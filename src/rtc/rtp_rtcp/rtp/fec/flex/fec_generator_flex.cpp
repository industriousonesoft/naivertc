#include "rtc/rtp_rtcp/rtp/fec/flex/fec_generator_flex.hpp"

namespace naivertc {

FlexfecGenerator::FlexfecGenerator() {

}

FlexfecGenerator::~FlexfecGenerator() = default;

size_t FlexfecGenerator::MaxPacketOverhead() const {
    return 0;
}

void FlexfecGenerator::SetProtectionParameters(const FecProtectionParams& delta_params, const FecProtectionParams& key_params) {

}

void FlexfecGenerator::PushMediaPacket(std::shared_ptr<RtpPacketToSend> packet) {

}

std::vector<std::shared_ptr<RtpPacketToSend>> FlexfecGenerator::PopFecPackets() {
    std::vector<std::shared_ptr<RtpPacketToSend>> fec_packets; 

    return fec_packets;
}
    
} // namespace naivertc
