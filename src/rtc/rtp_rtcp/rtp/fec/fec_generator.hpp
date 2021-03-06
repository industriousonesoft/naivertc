#ifndef _RTC_RTP_RTCP_FEC_GENERATOR_H_
#define _RTC_RTP_RTCP_FEC_GENERATOR_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"

#include <optional>

namespace naivertc {

class FecGenerator {
public:
    enum class FecType {
        ULP_FEC,
        FLEX_FEC
    };

public:
    virtual ~FecGenerator() = default;

    virtual FecType fec_type() const = 0;

    virtual std::optional<int> red_payload_type() = 0;
    
    virtual std::optional<uint32_t> fec_ssrc() = 0;

    virtual size_t MaxPacketOverhead() const  = 0;

    virtual void SetProtectionParameters(const FecProtectionParams& delta_params, 
                                         const FecProtectionParams& key_params) = 0;

    // Push a packt to be protected, and the generated FEC packets will be stored inside
    virtual void PushMediaPacket(RtpPacketToSend packet) = 0;

    // Pop out FEC packets pending in the generator.
    // TODO: To assign sequnce number for FEC packets internally 
    virtual std::vector<RtpPacketToSend> PopFecPackets() = 0;
};
    
} // namespace naivertc


#endif