#ifndef _RTC_RTP_RTCP_RTP_PACKET_GENERATOR_IMPL_H_
#define _RTC_RTP_RTCP_RTP_PACKET_GENERATOR_IMPL_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extension_manager.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"

#include <memory>
#include <vector>
#include <optional>
#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketGenerator {
public:
    RtpPacketGenerator(const RtpConfiguration& config);
    RtpPacketGenerator() = delete;
    RtpPacketGenerator(const RtpPacketGenerator&) = delete;
    RtpPacketGenerator& operator=(const RtpPacketGenerator&) = delete;
    ~RtpPacketGenerator();

    uint32_t ssrc() const;

    size_t max_rtp_packet_size() const;
    void set_max_rtp_packet_size(size_t max_size);
    
    RtpPacketToSend AllocatePacket() const;

    // RTX
    std::optional<uint32_t> rtx_ssrc() const;
    void SetRtxPayloadType(int payload_type, int associated_payload_type);
    std::optional<RtpPacketToSend> BuildRtxPacket(const RtpPacketToSend& packet);

    // Maximum header overhead per fec/padding packet.
    size_t FecOrPaddingPacketMaxRtpHeaderSize() const;

private:
    void UpdateHeaderSizes();

    int32_t ResendPacket(uint16_t packet_id);
    static void CopyHeaderAndExtensionsToRtxPacket(const RtpPacketToSend& packet, 
                                                   RtpPacketToSend* rtx_packet);

private:
    SequenceChecker sequence_checker_;
    const uint32_t ssrc_;
    std::optional<uint32_t> rtx_ssrc_;
    size_t max_packet_size_;
    size_t max_padding_fec_packet_header_;

    rtp::HeaderExtensionManager extension_manager_;

    std::map<int8_t, int8_t> rtx_payload_type_map_;
    std::vector<uint32_t> csrcs_;

};
    
} // namespace naivertc


#endif