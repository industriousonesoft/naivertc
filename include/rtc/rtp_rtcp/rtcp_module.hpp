#ifndef _RTC_RTP_RTCP_RTCP_SENCEIVER_H_
#define _RTC_RTP_RTCP_RTCP_SENCEIVER_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtcpModule : public RtcpReceiver::Observer,
                                  public RtpSentStatisticsObserver,
                                  public NackSender,
                                  public KeyFrameRequestSender {
public:
    RtcpModule(const RtcpConfiguration& config, 
               std::shared_ptr<TaskQueue> task_queue);
    ~RtcpModule();

    void set_rtt_ms(int64_t rtt_ms);
    int64_t rtt_ms() const;

    void IncomingPacket(const uint8_t* packet, size_t packet_size);
    void IncomingPacket(CopyOnWriteBuffer rtcp_packet);

    // NackSender override methods
    void SendNack(std::vector<uint16_t> nack_list,
                  bool buffering_allowed) override;

    // KeyFrameRequestSender override methods
    void RequestKeyFrame() override;

    int32_t RTT(uint32_t remote_ssrc,
                int64_t* last_rtt_ms,
                int64_t* avg_rtt_ms,
                int64_t* min_rtt_ms,
                int64_t* max_rtt_ms) const;

    int32_t RemoteNTP(uint32_t* received_ntp_secs,
                      uint32_t* received_ntp_frac,
                      uint32_t* rtcp_arrival_time_secs,
                      uint32_t* rtcp_arrival_time_frac,
                      uint32_t* rtcp_timestamp) const;

private:
    // RtpSentStatistics Observer
    void RtpSentCountersUpdated(const RtpSentCounters& rtp_sent_counters, 
                                const RtpSentCounters& rtx_sent_counters) override;
    void RtpSentBitRateUpdated(const BitRate bit_rate) override;

private:
    // RtcpSender
    const RtcpSender::FeedbackState& GetFeedbackState();
    void MaybeSendRtcp();
    void ScheduleRtcpSendEvaluation(TimeDelta duration);
    void MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time);
private:
    // RtcpReceiver observer methods
    void SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) override;
    void OnRequestSendReport() override;
    void OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers) override;
    void OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks) override;  
private:
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<TaskQueue> task_queue_;
    
    RtcpSender rtcp_sender_;
    RtcpReceiver rtcp_receiver_;
    TaskQueue work_queue_;

    RtcpSender::FeedbackState feedback_state_;

    int64_t rtt_ms_;
};
    
} // namespace naivertc


#endif