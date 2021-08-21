#ifndef _RTC_RTP_RTCP_RTP_SENDER_H_
#define _RTC_RTP_RTCP_RTP_SENDER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interface.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_packet_history.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_packet_sender.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_packet_sequencer.hpp"

#include <memory>
#include <vector>
#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT RtpSender {
public:
    RtpSender(const RtpRtcpInterface::Configuration& config, 
              RtpPacketHistory* packet_history, 
              RtpPacketSender* packet_sender,
              std::shared_ptr<TaskQueue> task_queue);
    RtpSender() = delete;
    RtpSender(const RtpSender&) = delete;
    RtpSender& operator=(const RtpSender&) = delete;
    ~RtpSender();

    uint32_t ssrc() const;

    size_t max_packet_size() const;
    void set_max_packet_size(size_t max_size);

    bool SendToNetwork(std::shared_ptr<RtpPacketToSend> packet);
    void EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets);

    // NACK
    void OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t avg_rrt);
    int32_t ResendPacket(uint16_t packet_id);

    // RTX
    RtxMode rtx_mode() const;
    void set_rtx_mode(RtxMode mode);
    std::optional<uint32_t> rtx_ssrc() const;
    void SetRtxPayloadType(int payload_type, int associated_payload_type);

private:
    std::shared_ptr<RtpPacketToSend> BuildRtxPacket(std::shared_ptr<const RtpPacketToSend>);

    void UpdateHeaderSizes();

    static void CopyHeaderAndExtensionsToRtxPacket(std::shared_ptr<const RtpPacketToSend>, RtpPacketToSend* rtx_packet);

private:
    std::shared_ptr<TaskQueue> task_queue_;
    std::shared_ptr<Clock> clock_;
    const uint32_t ssrc_;
    std::optional<uint32_t> rtx_ssrc_;
    RtxMode rtx_mode_;

    size_t max_packet_size_;

    RtpPacketHistory* const packet_history_;
    RtpPacketSender* const paced_sender_;

    RtpPacketSequencer sequencer_;

    std::map<int8_t, int8_t> rtx_payload_type_map_;
    std::vector<uint32_t> csrcs_;
};
    
} // namespace naivertc


#endif