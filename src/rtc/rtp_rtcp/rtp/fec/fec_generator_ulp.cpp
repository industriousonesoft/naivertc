#include "rtc/rtp_rtcp/rtp/fec/fec_generator_ulp.hpp"

namespace naivertc {

UlpFecGenerator::UlpFecGenerator(int red_payload_type, int fec_payload_type, std::shared_ptr<Clock> clock) {

}
    
UlpFecGenerator::~UlpFecGenerator() {

}

size_t UlpFecGenerator::MaxPacketOverhead() const {
    return 0;
}

void UlpFecGenerator::SetProtectionParameters(const FecProtectionParams& delta_params, const FecProtectionParams& key_params) {

}

void UlpFecGenerator::PushPacketToGenerateFec(std::shared_ptr<RtpPacketToSend> packet) {

}

void UlpFecGenerator::PopFecPackets() {

}
    
} // namespace naivertc
