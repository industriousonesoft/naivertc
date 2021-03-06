#ifndef _RTC_RTP_RTCP_FEC_FLEX_FEC_GENERATOR_H_
#define _RTC_RTP_RTCP_FEC_FLEX_FEC_GENERATOR_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"

namespace naivertc {

class FlexfecGenerator : public FecGenerator {
public:
    FlexfecGenerator(int payload_type,
                     uint32_t ssrc,
                     uint32_t protected_media_ssrc);
    ~FlexfecGenerator();

    FecType fec_type() const override { return FecGenerator::FecType::FLEX_FEC; };

    std::optional<uint32_t> fec_ssrc() override { return ssrc_; };

    std::optional<int> red_payload_type() override { return std::nullopt; };

    size_t MaxPacketOverhead() const override;

    void SetProtectionParameters(const FecProtectionParams& delta_params, const FecProtectionParams& key_params) override;

    void PushMediaPacket(RtpPacketToSend packet) override;

    std::vector<RtpPacketToSend> PopFecPackets() override;

private:
    const int payload_type_;
    const uint32_t ssrc_;
    const uint32_t protected_media_ssrc_;
};
    
} // namespace naivertc


#endif