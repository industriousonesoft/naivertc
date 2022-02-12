#ifndef _RTC_CALL_RTP_SEND_CONTROLLER_H_
#define _RTC_CALL_RTP_SEND_CONTROLLER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/congestion_controller/network_controller_interface.hpp"

#include <unordered_map>

namespace naivertc {

class Clock;

class RTC_CPP_EXPORT RtpSendController : public RtcpBandwidthObserver,
                                         public RtcpReportBlocksObserver,
                                         public RtcpTransportFeedbackObserver,
                                         public RtpTransportFeedbackObserver {
public:
    RtpSendController(Clock* clock);
    ~RtpSendController() override;

private:
    // Implements RtcpBandwidthObserver
    void OnReceivedEstimatedBitrateBps(uint32_t bitrate_bps) override;
    // Implements RtcpReportBlocksObserver
    void OnReceivedRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                    int64_t rtt_ms) override;
    // Implements RtcpTransportFeedbackObserver
    void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;
    // Implements RtpTransportFeedbackObserver
    void OnAddPacket(const RtpTransportFeedback& feedback) override;

private:
    void OnNetworkControlUpdate(NetworkControlUpdate update);

    void HandleRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                Timestamp now);

private:
    Clock* const clock_;
    TaskQueue worker_queue_;

    std::unique_ptr<NetworkControllerInterface> network_controller_;

    Timestamp last_report_block_time_;
    std::unordered_map<uint32_t, RtcpReportBlock> last_report_blocks_;
};
    
} // namespace naivertc

#endif