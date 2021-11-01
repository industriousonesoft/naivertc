#ifndef _RTC_RTP_RTCP_FEC_UPL_FEC_GENERATOR_H_
#define _RTC_RTP_RTCP_FEC_UPL_FEC_GENERATOR_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_encoder.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"

#include <optional>

namespace naivertc {

// NOTE: This class is not thread safe, the caller MUST provide that.
class RTC_CPP_EXPORT UlpFecGenerator : public FecGenerator {
public:
    UlpFecGenerator(int red_payload_type, 
                    int fec_payload_type);
    virtual ~UlpFecGenerator();

    FecType fec_type() const override { return FecGenerator::FecType::ULP_FEC; }

    // TODO: Support both of RED and new stream
    std::optional<uint32_t> fec_ssrc() override { return std::nullopt; };

    std::optional<int> red_payload_type() override { return red_payload_type_; };

    size_t MaxPacketOverhead() const override;

    void SetProtectionParameters(const FecProtectionParams& delta_params, const FecProtectionParams& key_params) override;

    void PushMediaPacket(std::shared_ptr<RtpPacketToSend> packet) override;

    std::vector<std::shared_ptr<RtpPacketToSend>> PopFecPackets() override;

protected:
    const FecProtectionParams& CurrentParams() const;

    bool MaxExcessOverheadNotReached(size_t target_fec_rate) const;
    bool MinimumMediaPacketsReached() const;

    void Reset();
    
private:
    int red_payload_type_;
    int fec_payload_type_;
    
    size_t num_protected_frames_;
    size_t min_num_media_packets_;
    bool contains_key_frame_;
    const std::unique_ptr<FecEncoder> fec_encoder_;

    using ParamsTuple = std::pair<FecProtectionParams, FecProtectionParams>;
    ParamsTuple current_params_;
    std::optional<ParamsTuple> pending_params_;

    std::optional<std::shared_ptr<RtpPacketToSend>> last_protected_media_packet_;
    FecEncoder::PacketList media_packets_;
    FecEncoder::FecPacketList generated_fec_packets_;
    size_t num_generated_fec_packets_;
};
    
} // namespace naivert 


#endif