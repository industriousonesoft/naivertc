#ifndef _RTC_RTP_RTCP_RTP_PACKET_GENERATOR_IMPL_H_
#define _RTC_RTP_RTCP_RTP_PACKET_GENERATOR_IMPL_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extension_manager.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_sender.hpp"

#include <memory>
#include <vector>
#include <optional>
#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketGenerator {
public:
    RtpPacketGenerator(const RtpSender::Configuration& config,
                       std::shared_ptr<TaskQueue> task_queue);
    RtpPacketGenerator() = delete;
    RtpPacketGenerator(const RtpPacketGenerator&) = delete;
    RtpPacketGenerator& operator=(const RtpPacketGenerator&) = delete;
    ~RtpPacketGenerator();

    uint32_t ssrc() const;

    size_t max_rtp_packet_size() const;
    void set_max_rtp_packet_size(size_t max_size);
    
    std::shared_ptr<RtpPacketToSend> AllocatePacket() const;

    // RTX
    std::optional<uint32_t> rtx_ssrc() const;
    void SetRtxPayloadType(int payload_type, int associated_payload_type);
    std::shared_ptr<RtpPacketToSend> BuildRtxPacket(std::shared_ptr<const RtpPacketToSend>);

    // Maximum header overhead per fec/padding packet.
    size_t FecOrPaddingPacketMaxRtpHeaderSize() const;

private:
    void UpdateHeaderSizes();

    int32_t ResendPacket(uint16_t packet_id);
    static void CopyHeaderAndExtensionsToRtxPacket(std::shared_ptr<const RtpPacketToSend>, RtpPacketToSend* rtx_packet);

private:
    const uint32_t ssrc_;
    std::optional<uint32_t> rtx_ssrc_;
    size_t max_packet_size_;
    size_t max_padding_fec_packet_header_;

    std::shared_ptr<TaskQueue> task_queue_;

    std::shared_ptr<rtp::ExtensionManager> extension_manager_;

    std::map<int8_t, int8_t> rtx_payload_type_map_;
    std::vector<uint32_t> csrcs_;

};
    
} // namespace naivertc


#endif