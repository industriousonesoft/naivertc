#ifndef _RTC_CALL_RTP_SEND_CONTROLLER_H_
#define _RTC_CALL_RTP_SEND_CONTROLLER_H_

#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/task_utils/repeating_task.hpp"
#include "rtc/congestion_control/components/network_transport_statistician.hpp"
#include "rtc/congestion_control/pacing/task_queue_paced_sender.hpp"
#include "rtc/congestion_control/send_side/network_controller_interface.hpp"

#include <unordered_map>

namespace naivertc {

class Clock;
class NetworkControllerInterface;

class RtpSendController : public RtcpBandwidthObserver,
                          public RtcpTransportFeedbackObserver,
                          public RtpTransportFeedbackObserver {
public:
    struct Configuration {
        Clock* clock;

        // Taraget bitrate settings.
        std::optional<DataRate> min_bitrate;
        std::optional<DataRate> max_bitrate;
        std::optional<DataRate> starting_bitrate;
    };
public:
    RtpSendController(const Configuration& config);
    ~RtpSendController() override;

    void Clear();

    void OnNetworkAvailability(bool network_available);

    // void OnReceivedPacket(const ReceivedPacket& recv_packet);

    // Implements RtpTransportFeedbackObserver
    void OnAddPacket(const RtpPacketSendInfo& feedback) override;
    void OnSentPacket(const RtpSentPacket& sent_packet) override;

    // Implements RtcpBandwidthObserver
    void OnReceivedEstimatedBitrateBps(uint32_t bitrate_bps) override;

    // Implements RtcpTransportFeedbackObserver
    void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;
    void OnReceivedRtcpReceiveReport(const std::vector<RtcpReportBlock>& report_blocks,
                                     int64_t rtt_ms) override;

private:
    void MaybeCreateNetworkController();

    void PostUpdates(NetworkControlUpdate update);

    void HandleRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                Timestamp now);

    void StartPeriodicTasks();
    void UpdatePeriodically();

private:
    Clock* const clock_;
    TaskQueue task_queue_;
    TaskQueue pacing_queue_;
    std::unique_ptr<RepeatingTask> controller_task_;

    bool network_available_ RTC_GUARDED_BY(task_queue_);

    NetworkTransportStatistician transport_statistician_ RTC_GUARDED_BY(task_queue_);

    NetworkControllerInterface::Configuration network_config_ RTC_GUARDED_BY(task_queue_);
    std::unique_ptr<NetworkControllerInterface> network_controller_ RTC_GUARDED_BY(task_queue_);

    std::unique_ptr<TaskQueuePacedSender> pacer_ RTC_GUARDED_BY(task_queue_);

    Timestamp last_report_block_time_ RTC_GUARDED_BY(task_queue_);
    std::unordered_map<uint32_t, RtcpReportBlock> last_report_blocks_ RTC_GUARDED_BY(task_queue_);
};
    
} // namespace naivertc

#endif