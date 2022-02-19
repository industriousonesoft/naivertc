#ifndef _RTC_RTP_RTCP_RTP_PACKET_GENERATOR_IMPL_H_
#define _RTC_RTP_RTCP_RTP_PACKET_GENERATOR_IMPL_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"

#include <memory>
#include <vector>
#include <optional>
#include <functional>

namespace naivertc {

class SequenceNumberAssigner;

namespace rtp {
class HeaderExtensionMap;
}

class RtpPacketGenerator {
public:
    RtpPacketGenerator(const RtpConfiguration& config, 
                       rtp::HeaderExtensionMap* header_extension_map);
    RtpPacketGenerator() = delete;
    RtpPacketGenerator(const RtpPacketGenerator&) = delete;
    RtpPacketGenerator& operator=(const RtpPacketGenerator&) = delete;
    ~RtpPacketGenerator();

    uint32_t ssrc() const;
    
    void set_csrcs(const std::vector<uint32_t>& csrcs);

    size_t max_rtp_packet_size() const;
    void set_max_rtp_packet_size(size_t max_size);
    
    RtpPacketToSend GeneratePacket() const;

    // RTX
    std::optional<uint32_t> rtx_ssrc() const;
    void SetRtxPayloadType(int payload_type, int associated_payload_type);
    std::optional<RtpPacketToSend> BuildRtxPacket(const RtpPacketToSend& packet);

    // Return the maximum header size per media packet.
    size_t MaxMediaPacketHeaderSize() const;

    // Maximum header size per fec/padding packet.
    size_t MaxFecOrPaddingPacketHeaderSize() const;

private:
    void UpdateHeaderSizes();

    int32_t ResendPacket(uint16_t packet_id);
    static void CopyHeaderAndExtensionsToRtxPacket(const RtpPacketToSend& packet, 
                                                   RtpPacketToSend* rtx_packet);

private:
    SequenceChecker sequence_checker_;
    bool is_audio_;
    const uint32_t ssrc_;
    std::optional<uint32_t> rtx_ssrc_;
    size_t max_packet_size_;
    size_t max_media_packet_header_size_;
    size_t max_fec_or_padding_packet_header_size_;
    rtp::HeaderExtensionMap* const header_extension_map_;

    std::map<int8_t, int8_t> rtx_payload_type_map_;
    std::vector<uint32_t> csrcs_;

};
    
} // namespace naivertc


#endif