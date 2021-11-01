#ifndef _RTC_RTP_RTCP_RTP_PACKET_SENDER_IMPL_H_
#define _RTC_RTP_RTCP_RTP_PACKET_SENDER_IMPL_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/base/repeating_task.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_structs.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"
#include "rtc/rtp_rtcp/components/bit_rate_statistics.hpp"

#include <optional>
#include <functional>
#include <vector>
#include <map>

namespace naivertc {

// NOTE: PacedSender 和 NonPacedsender最终都是通过RtpPacketEgresser发送数据，不同在于二者的发送逻辑不同，包括发送步幅和处理fec包等
class RTC_CPP_EXPORT RtpPacketEgresser {
public:
    RtpPacketEgresser(const RtpConfiguration& config,
                    RtpPacketSentHistory* const packet_history,
                    FecGenerator* const fec_generator,
                    std::shared_ptr<TaskQueue> task_queue);
    ~RtpPacketEgresser();

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

    void UpdateSentStatistics(const int64_t now_ms, const RtpPacketToSend& packet);

    // Return total bitrates for all kind sent packets for now.
    const BitRate CalcTotalSentBitRate(const int64_t now_ms);

    void PeriodicUpdate();
  
private:
    friend class NonPacedPacketSender;

    std::shared_ptr<Clock> clock_; 
    const uint32_t ssrc_;
    const std::optional<uint32_t> rtx_ssrc_;
    RtpSentStatisticsObserver* rtp_sent_statistics_observer_;
    
    RtpPacketSentHistory* const packet_history_;
    FecGenerator* const fec_generator_;
    std::shared_ptr<TaskQueue> task_queue_;

    std::optional<std::pair<FecProtectionParams, FecProtectionParams>> pending_fec_params_;

    bool media_has_been_sent_ = false;

    RtpSentCounters rtp_sent_counters_;
    RtpSentCounters rtx_sent_counters_;
    std::map<RtpPacketType, BitRateStatistics> send_bitrate_map_;

    std::shared_ptr<TaskQueue> worker_queue_;
    std::unique_ptr<RepeatingTask> update_task_;
};
    
} // namespace naivertc


#endif