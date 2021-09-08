#ifndef _RTC_RTP_RTCP_RTCP_SENCEIVER_H_
#define _RTC_RTP_RTCP_RTCP_SENCEIVER_H_

#include "base/defines.hpp"
#include "rtc/base/clock.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtcpModule : public RtcpReceiver::Observer,
                                     public RtpSentStatisticsObserver {
public:
    RtcpModule(const RtcpConfiguration& config, 
                   std::shared_ptr<TaskQueue> task_queue);
    ~RtcpModule();

private:
    // RtpSentStatisticsObserver
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
    // RtcpReceiver
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
};
    
} // namespace naivertc


#endif