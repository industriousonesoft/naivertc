#ifndef _RTC_RTP_RTCP_RTP_PACKET_SENDER_IMPL_H_
#define _RTC_RTP_RTCP_RTP_PACKET_SENDER_IMPL_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interface.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"

#include <optional>
#include <functional>
#include <vector>
// 
namespace naivertc {

// NOTE: PacedSender 和 NonPacedsender最终都是通过RtpPacketSender发送数据，不同在于二者的发送逻辑不同，包括发送步幅和处理fec包等
class RTC_CPP_EXPORT RtpPacketSenderEgress {
public:
    RtpPacketSenderEgress(const RtpRtcpInterface::Configuration& config,
                          RtpPacketSentHistory* const packet_history,
                          std::shared_ptr<TaskQueue> task_queue);
    ~RtpPacketSenderEgress();

    uint32_t ssrc() const { return ssrc_; }
    std::optional<uint32_t> rtx_ssrc() const { return rtx_ssrc_; }
   
    void SetFecProtectionParameters(const FecProtectionParams& delta_params,
                                    const FecProtectionParams& key_params);

    void SendPacket(std::shared_ptr<RtpPacketToSend> packet);

    std::vector<std::shared_ptr<RtpPacketToSend>> FetchFecPackets() const;

private:
    bool SendPacketToNetwork(std::shared_ptr<RtpPacketToSend> packet);

    bool HasCorrectSsrc(std::shared_ptr<RtpPacketToSend> packet);

    void SendPacketToNetworkFeedback(uint16_t packet_id, std::shared_ptr<RtpPacketToSend> packet);
    void UpdateDelayStatistics(int64_t capture_time_ms, int64_t now_ms, uint32_t ssrc);
    void OnSendPacket(uint16_t packet_id, int64_t capture_time_ms, uint32_t ssrc);

    void UpdateRtpStats(int64_t now_ms,
                        uint32_t packet_ssrc,
                        RtpPacketType packet_type,
                        size_t packet_size);

private:
    std::shared_ptr<Clock> clock_; 
    const uint32_t ssrc_;
    const std::optional<uint32_t> rtx_ssrc_;
    RtpPacketSentHistory* const packet_history_;
    std::shared_ptr<FecGenerator> fec_generator_;
    std::optional<std::pair<FecProtectionParams, FecProtectionParams>> pending_fec_params_;

    std::shared_ptr<TaskQueue> task_queue_;

    bool media_has_been_sent_ = false;
};
    
} // namespace naivertc


#endif