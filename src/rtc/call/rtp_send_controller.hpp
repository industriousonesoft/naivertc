#ifndef _RTC_CALL_RTP_SEND_CONTROLLER_H_
#define _RTC_CALL_RTP_SEND_CONTROLLER_H_

#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/task_utils/repeating_task.hpp"
#include "rtc/congestion_control/controllers/network_controller_interface.hpp"
#include "rtc/congestion_control/components/network_transport_statistician.hpp"

#include <unordered_map>

namespace naivertc {

class Clock;

class RtpSendController : public RtcpBandwidthObserver,
                                         public RtcpTransportFeedbackObserver,
                                         public RtpTransportFeedbackObserver {
public:
    RtpSendController(Clock* clock);
    ~RtpSendController() override;

private:
    // Implements RtpTransportFeedbackObserver
    void OnAddPacket(const RtpPacketSendInfo& feedback) override;
    void OnSentPacket(const RtpSentPacket& sent_packet) override;

    // Implements RtcpBandwidthObserver
    void OnReceivedEstimatedBitrateBps(uint32_t bitrate_bps) override;

    // Implements RtcpTransportFeedbackObserver
    void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;
    void OnReceivedRtcpReceiveReport(const std::vector<RtcpReportBlock>& report_blocks,
                                     int64_t rtt_ms) override;
  
    void UpdatePeriodically();

private:
    void OnNetworkControlUpdate(NetworkControlUpdate update);

    void HandleRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                Timestamp now);

private:
    Clock* const clock_;
    TaskQueue worker_queue_;
    TimeDelta update_interval_;
    std::unique_ptr<RepeatingTask> controller_task_;

    NetworkTransportStatistician transport_statistician_;
    std::unique_ptr<NetworkControllerInterface> network_controller_;

    Timestamp last_report_block_time_;
    std::unordered_map<uint32_t, RtcpReportBlock> last_report_blocks_;
};
    
} // namespace naivertc

#endif