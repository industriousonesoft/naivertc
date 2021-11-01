#ifndef _RTC_RTP_RTCP_FEC_FLEX_FEC_GENERATOR_H_
#define _RTC_RTP_RTCP_FEC_FLEX_FEC_GENERATOR_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"

namespace naivertc {

class RTC_CPP_EXPORT FlexfecGenerator : public FecGenerator {
public:
    FlexfecGenerator();
    ~FlexfecGenerator();

    FecType fec_type() const override { return FecGenerator::FecType::FLEX_FEC; };

    std::optional<uint32_t> fec_ssrc() override { return std::nullopt; };

    std::optional<int> red_payload_type() override { return std::nullopt; };

    size_t MaxPacketOverhead() const override;

    void SetProtectionParameters(const FecProtectionParams& delta_params, const FecProtectionParams& key_params) override;

    void PushMediaPacket(std::shared_ptr<RtpPacketToSend> packet) override;

    std::vector<std::shared_ptr<RtpPacketToSend>> PopFecPackets() override;
};
    
} // namespace naivertc


#endif