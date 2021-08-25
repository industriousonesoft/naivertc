#ifndef _RTC_RTP_RTCP_RTP_SENDER_H_
#define _RTC_RTP_RTCP_RTP_SENDER_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interface.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sender.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.hpp"

#include <memory>
#include <vector>
#include <optional>
#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketGenerator : public RtpPacketSender {
public:
    RtpPacketGenerator(const RtpRtcpInterface::Configuration& config,
                       std::shared_ptr<RtpPacketSender> lower,
                       std::shared_ptr<RtpPacketSentHistory> packet_history,
                       std::shared_ptr<TaskQueue> task_queue);
    RtpPacketGenerator() = delete;
    RtpPacketGenerator(const RtpPacketGenerator&) = delete;
    RtpPacketGenerator& operator=(const RtpPacketGenerator&) = delete;
    ~RtpPacketGenerator();

    uint32_t ssrc() const;

    size_t max_packet_size() const;
    void set_max_packet_size(size_t max_size);

    void EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) override;

    // NACK
    void OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t avg_rrt);
    
    // RTX
    RtxMode rtx_mode() const;
    void set_rtx_mode(RtxMode mode);
    std::optional<uint32_t> rtx_ssrc() const;
    void SetRtxPayloadType(int payload_type, int associated_payload_type);

    std::shared_ptr<SequenceNumberAssigner> sequence_num_assigner() const;

private:
    std::shared_ptr<RtpPacketToSend> BuildRtxPacket(std::shared_ptr<const RtpPacketToSend>);
    int32_t ResendPacket(uint16_t packet_id);

    void UpdateHeaderSizes();

    static void CopyHeaderAndExtensionsToRtxPacket(std::shared_ptr<const RtpPacketToSend>, RtpPacketToSend* rtx_packet);

private:
    std::shared_ptr<Clock> clock_;
    const uint32_t ssrc_;
    std::optional<uint32_t> rtx_ssrc_;
    RtxMode rtx_mode_;
    size_t max_packet_size_;

    std::shared_ptr<RtpPacketSender> lower_;
    std::shared_ptr<RtpPacketSentHistory> packet_history_;
    std::shared_ptr<TaskQueue> task_queue_;

    std::shared_ptr<RtpPacketSequencer> sequencer_;

    std::map<int8_t, int8_t> rtx_payload_type_map_;
    std::vector<uint32_t> csrcs_;
};
    
} // namespace naivertc


#endif