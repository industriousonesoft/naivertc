#ifndef _RTC_RTP_RTCP_FEC_UPL_FEC_GENERATOR_H_
#define _RTC_RTP_RTCP_FEC_UPL_FEC_GENERATOR_H_

#include "base/defines.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"

namespace naivertc {

class RTC_CPP_EXPORT UlpFecGenerator final : public FecGenerator {
public:
    UlpFecGenerator(int red_payload_type, int fec_payload_type, std::shared_ptr<Clock> clock);
    ~UlpFecGenerator();

    FecType fec_type() const override { return FecGenerator::FecType::ULP_FEC; }

    // TODO: Support both of RED and new stream
    std::optional<uint32_t> fec_ssrc() override { return std::nullopt; };

    size_t MaxPacketOverhead() const override;

    void SetProtectionParameters(const FecProtectionParams& delta_params, const FecProtectionParams& key_params) override;

    void PushPacketToGenerateFec(std::shared_ptr<RtpPacketToSend> packet) override;

    void PopFecPackets() override;
};
    
} // namespace naivert 


#endif