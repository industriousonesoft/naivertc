#ifndef _RTC_RTP_RTCP_RTP_PACKET_GENERATOR_IMPL_H_
#define _RTC_RTP_RTCP_RTP_PACKET_GENERATOR_IMPL_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extension_map.hpp"

#include <memory>
#include <vector>
#include <optional>
#include <functional>
#include <string>

namespace naivertc {

class SequenceNumberAssigner;
class RtpPacketHistory;

class RtpPacketGenerator {
public:
    RtpPacketGenerator(const RtpConfiguration& config,
                       RtpPacketHistory* packet_history);
    RtpPacketGenerator() = delete;
    RtpPacketGenerator(const RtpPacketGenerator&) = delete;
    RtpPacketGenerator& operator=(const RtpPacketGenerator&) = delete;
    ~RtpPacketGenerator();

    uint32_t media_ssrc() const;
    
    void set_csrcs(const std::vector<uint32_t>& csrcs);

    size_t max_rtp_packet_size() const;
    void set_max_rtp_packet_size(size_t max_size);

    void set_mid(const std::string& mid);
    void set_rid(const std::string& rid);

    // Rtp header extensions
    bool Register(RtpExtensionType type, int id);
    bool Register(std::string_view uri, int id);
    bool IsRegistered(RtpExtensionType type);
    void Deregister(std::string_view uri);

    // Generate
    RtpPacketToSend GeneratePacket() const;

    // RTX
    int rtx_mode() const;
    void set_rtx_mode(int mode);
    std::optional<uint32_t> rtx_ssrc() const;
    void SetRtxPayloadType(int payload_type, int associated_payload_type);
    std::optional<RtpPacketToSend> BuildRtxPacket(const RtpPacketToSend& packet);

    // Padding
    bool SupportsPadding() const;
    bool SupportsRtxPayloadPadding() const;
    std::vector<RtpPacketToSend> GeneratePadding(size_t target_packet_size, 
                                                 bool media_has_been_sent,
                                                 bool can_send_padding_on_medai_ssrc);

    // Return the maximum header size per media packet.
    size_t MaxMediaPacketHeaderSize() const;
    // Maximum header size per fec/padding packet.
    size_t MaxFecOrPaddingPacketHeaderSize() const;

    void UpdateHeaderSizes();

    void OnReceivedAckOnMediaSsrc();
    void OnReceivedAckOnRtxSsrc();

private:
    int32_t ResendPacket(uint16_t packet_id);
    static void CopyHeaderAndExtensionsToRtxPacket(const RtpPacketToSend& packet, 
                                                   RtpPacketToSend* rtx_packet);

private:
    SequenceChecker sequence_checker_;
    const bool is_audio_;
    const uint32_t media_ssrc_;
    std::optional<uint32_t> rtx_ssrc_;
    int rtx_mode_;
    size_t max_packet_size_;
    size_t max_media_packet_header_size_;
    size_t max_fec_or_padding_packet_header_size_;
    const double max_padding_size_factor_;
    const bool always_send_mid_and_rid_;
    // Mapping rtx_payload_type_map_[associated] = rtx.
    rtp::HeaderExtensionMap rtp_header_extension_map_;

    RtpPacketHistory* const packet_history_;

    // RID value to send in the RID or RepairedRID header extension.
    std::string rid_;
    // MID value to send in the MID header extension.
    std::string mid_;

    bool supports_bwe_extension_ = false;
    bool media_ssrc_has_acked_ = false;
    bool rtx_ssrc_has_acked_ = false;

    std::map<int8_t, int8_t> rtx_payload_type_map_;
    std::vector<uint32_t> csrcs_;
};
    
} // namespace naivertc


#endif