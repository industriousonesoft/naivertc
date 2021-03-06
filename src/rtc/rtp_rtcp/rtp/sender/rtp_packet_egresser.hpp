#ifndef _RTC_RTP_RTCP_RTP_PACKET_SENDER_IMPL_H_
#define _RTC_RTP_RTCP_RTP_PACKET_SENDER_IMPL_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_statistic_types.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/components/bit_rate_statistics.hpp"
#include "rtc/base/task_utils/queued_task.hpp"

#include <optional>
#include <functional>
#include <vector>
#include <unordered_map>

namespace naivertc {

class RepeatingTask;
class RtpPacketHistory;
class RtpPacketSequencer;

class RtpPacketEgresser {
public:
    // NonPacedPacketSender
    // NOTE: PacedSender 和 NonPacedsender最终都是通过RtpPacketEgresser发送数据，
    // 不同在于二者的发送逻辑不同，包括发送步幅和处理fec包等
    class NonPacedPacketSender final : public RtpPacketSender {
    public:
        NonPacedPacketSender(RtpPacketEgresser* const sender);
        ~NonPacedPacketSender() override;

        void EnqueuePackets(std::vector<RtpPacketToSend> packets) override;

    private:
        RtpPacketEgresser* const sender_;
    };
public:
    RtpPacketEgresser(const RtpConfiguration& config,
                      SequenceNumberAssigner* seq_num_assigner,
                      RtpPacketHistory* packet_history);
    ~RtpPacketEgresser();

    uint32_t ssrc() const;
    std::optional<uint32_t> rtx_ssrc() const;
    std::optional<uint32_t> flex_fec_ssrc() const;

    bool media_has_been_sent() const;

    void set_transport_seq_num(uint16_t seq_num);
   
    void SetFecProtectionParameters(const FecProtectionParams& delta_params,
                                    const FecProtectionParams& key_params);

    bool SendPacket(RtpPacketToSend packet,
                    std::optional<const PacedPacketInfo> pacing_info = std::nullopt);

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
    bool SendPacketToNetwork(RtpPacketToSend packet, PacketOptions options);

    bool VerifySsrcs(const RtpPacketToSend& packet);

    void AddPacketToTransportFeedback(uint16_t packet_id, 
                                      const RtpPacketToSend& packet,
                                      std::optional<const PacedPacketInfo> pacing_info);

    void UpdateSentStatistics(const int64_t now_ms, 
                              SendStats send_stats);
    void UpdateDelayStatistics(int64_t send_delay_ms,
                               int64_t now_ms,
                               uint32_t ssrc);

    DataRate CalcTotalSendBitrate(const int64_t now_ms);
    DataRate CalcSendBitrate(RtpPacketType packet_type, 
                             const int64_t now_ms);

    void RecalculateMaxDelay();

    void PeriodicUpdate();

    void PrepareForSend(RtpPacketToSend& packet);

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
    const std::optional<uint32_t> flex_fec_ssrc_;
    RtcMediaTransport* const send_transport_;
    
    RtpPacketHistory* const packet_history_;
    FecGenerator* const fec_generator_;
    SequenceNumberAssigner* const seq_num_assigner_;

    std::optional<std::pair<FecProtectionParams, FecProtectionParams>> pending_fec_params_;

    bool media_has_been_sent_ = false;
    uint64_t transport_sequence_number_;

    RtpStreamDataCounters rtp_send_counter_;
    RtpStreamDataCounters rtx_send_counter_;
    std::unordered_map<RtpPacketType, BitrateStatistics> send_bitrate_stats_;

    // The sum of delays over a sliding window.
    int64_t sliding_sum_delay_ms_ = 0;
    uint64_t accumulated_delay_ms_ = 0;
    SendDelayMap send_delays_;
    SendDelayMap::iterator max_delay_it_;

    TaskQueueImpl* worker_queue_;
    std::unique_ptr<RepeatingTask> update_task_;
    ScopedTaskSafety task_safety_;

    RtpSendDelayObserver* const send_delay_observer_;
    RtpSendPacketObserver* const send_packet_observer_;
    RtpSendBitratesObserver* const send_bitrates_observer_;
    RtpTransportFeedbackObserver* const transport_feedback_observer_;
    RtpStreamDataCountersObserver* const stream_data_counters_observer_;
};
    
} // namespace naivertc


#endif