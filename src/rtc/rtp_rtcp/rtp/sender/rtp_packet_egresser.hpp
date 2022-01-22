#ifndef _RTC_RTP_RTCP_RTP_PACKET_SENDER_IMPL_H_
#define _RTC_RTP_RTCP_RTP_PACKET_SENDER_IMPL_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/task_utils/repeating_task.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp_statistic_structs.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/components/bit_rate_statistics.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

#include <optional>
#include <functional>
#include <vector>
#include <unordered_map>

namespace naivertc {

class RtpPacketHistory;
class FecGenerator;

// NOTE: PacedSender 和 NonPacedsender最终都是通过RtpPacketEgresser发送数据，不同在于二者的发送逻辑不同，包括发送步幅和处理fec包等
class RTC_CPP_EXPORT RtpPacketEgresser {
public:
    RtpPacketEgresser(const RtpConfiguration& config,
                      RtpPacketHistory* const packet_history,
                      FecGenerator* const fec_generator);
    ~RtpPacketEgresser();

    uint32_t ssrc() const { return ssrc_; }
    std::optional<uint32_t> rtx_ssrc() const { return rtx_ssrc_; }
   
    void SetFecProtectionParameters(const FecProtectionParams& delta_params,
                                    const FecProtectionParams& key_params);

    void SendPacket(RtpPacketToSend packet);

    std::vector<RtpPacketToSend> FetchFecPackets() const;

    DataRate GetSendBitrate(RtpPacketType packet_type);
    // Return the total bitrates for all kind packets so far.
    DataRate GetTotalSendBitrate();

    RtpStreamDataCounters GetRtpStreamDataCounter() const;
    RtpStreamDataCounters GetRtxStreamDataCounter() const;

private:
    struct SendStats {
        SendStats(uint32_t ssrc,
                  size_t packet_size,
                  RtpPacketType packet_type,
                  RtpPacketCounter packet_counter)
            : ssrc(ssrc), 
              packet_size(packet_size),
              packet_type(packet_type),
              packet_counter(std::move(packet_counter)) {}

        uint32_t ssrc;
        size_t packet_size; 
        RtpPacketType packet_type;
        RtpPacketCounter packet_counter;
    };

private:
    bool SendPacketToNetwork(RtpPacketToSend packet);

    bool VerifySsrcs(const RtpPacketToSend& packet);

    void AddPacketToTransportFeedback(uint16_t packet_id, 
                                      const RtpPacketToSend& packet);

    void UpdateSentStatistics(const int64_t now_ms, 
                              SendStats send_stats);
    void UpdateDelayStatistics(int64_t capture_time_ms, 
                               int64_t now_ms, 
                               uint32_t ssrc);

    DataRate CalcTotalSendBitrate(const int64_t now_ms);
    DataRate CalcSendBitrate(RtpPacketType packet_type, 
                             const int64_t now_ms);

    void RecalculateMaxDelay();

    void PeriodicUpdate();
  
private:
    friend class NonPacedPacketSender;

    // The send-side delay is the difference between transmission time and capture time.
    using SendDelayMap = std::map</*now_ms=*/int64_t, /*delay_ms*/int64_t>;

    SequenceChecker sequence_checker_;
    const bool is_audio_;
    const bool send_side_bwe_with_overhead_;
    Clock* const clock_;
    const uint32_t ssrc_;
    const std::optional<uint32_t> rtx_ssrc_;
    MediaTransport* const send_transport_;
    
    RtpPacketHistory* const packet_history_;
    FecGenerator* const fec_generator_;

    std::optional<std::pair<FecProtectionParams, FecProtectionParams>> pending_fec_params_;

    bool media_has_been_sent_ = false;

    RtpStreamDataCounters rtp_send_counter_;
    RtpStreamDataCounters rtx_send_counter_;
    std::unordered_map<RtpPacketType, BitRateStatistics> send_bitrate_stats_;

    // The sum of delays over a sliding window.
    int64_t sliding_sum_delay_ms_;
    uint64_t accumulated_delay_ms_;
    SendDelayMap send_delays_;
    SendDelayMap::iterator max_delay_it_;

    TaskQueueImpl* worker_queue_;
    std::unique_ptr<RepeatingTask> update_task_;

    RtpSendDelayObserver* const send_delay_observer_;
    RtpSendPacketObserver* const send_packet_observer_;
    RtpSendBitratesObserver* const send_bitrates_observer_;
    RtpTransportFeedbackObserver* const transport_feedback_observer_;
    RtpStreamDataCountersObserver* const stream_data_counters_observer_;
};
    
} // namespace naivertc


#endif