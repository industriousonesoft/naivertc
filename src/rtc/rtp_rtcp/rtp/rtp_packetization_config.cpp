#include "rtc/rtp_rtcp/rtp/rtp_packetization_config.hpp"
#include "common/utils_random.hpp"

namespace naivertc {

RtpPacketizationConfig::RtpPacketizationConfig(uint32_t ssrc, std::string cname, 
                                               uint8_t payload_type, uint32_t clock_rate, 
                                               std::optional<uint16_t> sequence_num, 
                                               std::optional<uint32_t> timestamp) 
    : ssrc_(ssrc), cname_(cname), payload_type_(payload_type), clock_rate_(clock_rate) {
    assert(clock_rate > 0);
    sequence_num_ = sequence_num.value_or(utils::random::generate_random<uint16_t>());
    timestamp_ = timestamp.value_or(utils::random::generate_random<uint32_t>());
    start_timestamp_ = timestamp_;
}

void RtpPacketizationConfig::set_start_time(double start_time_s, EpochType epoch_type, std::optional<uint32_t> start_timestamp) {
    start_time_s_ = start_time_s_ + static_cast<unsigned long long>(epoch_type);
    if (start_timestamp.has_value()) {
        start_timestamp_ = start_timestamp.value();
        timestamp_ = start_timestamp_;
    }else {
        start_timestamp_ = timestamp_;
    }
}
    
} // namespace naivertc
