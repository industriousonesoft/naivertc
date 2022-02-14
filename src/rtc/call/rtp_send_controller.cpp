#include "rtc/call/rtp_send_controller.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/congestion_controller/goog_cc/goog_cc_network_controller.hpp"

namespace naivertc {

RtpSendController::RtpSendController(Clock* clock) 
    : clock_(clock),
      worker_queue_("RtpSendController.worker.queue"),
      network_controller_(std::make_unique<GoogCcNetworkController>(/*TODO: Set goog_cc configuration*/GoogCcNetworkController::Configuration())),
      last_report_block_time_(clock_->CurrentTime()) {}

RtpSendController::~RtpSendController() {}

// Private methods
void RtpSendController::OnReceivedEstimatedBitrateBps(uint32_t bitrate_bps) {
    DataRate bitrate = DataRate::BitsPerSec(bitrate_bps);
    Timestamp recv_time = Timestamp::Millis(clock_->now_ms());
    worker_queue_.Post([this, bitrate, recv_time](){
        if (network_controller_) {
            OnNetworkControlUpdate(network_controller_->OnRemoteBitrateUpdated(bitrate, recv_time));
        }
    });
}

void RtpSendController::OnReceivedRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                                   int64_t rtt_ms) {
    Timestamp now = clock_->CurrentTime();
    worker_queue_.Post([this, report_blocks, now](){
        HandleRtcpReportBlocks(report_blocks, now);
    });

    worker_queue_.Post([this, rtt_ms, now](){
        if (network_controller_ && rtt_ms > 0) {
            TimeDelta rtt = TimeDelta::Millis(rtt_ms);
            OnNetworkControlUpdate(network_controller_->OnRttUpdated(rtt, now));
        }
    });
}

void RtpSendController::OnTransportFeedback(const rtcp::TransportFeedback& feedback) {

}

void RtpSendController::OnSendFeedback(const RtpSendFeedback& feedback) {

}

void RtpSendController::OnNetworkControlUpdate(NetworkControlUpdate update) {
    RTC_RUN_ON(&worker_queue_);

}

void RtpSendController::HandleRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                               Timestamp now) {
    RTC_RUN_ON(&worker_queue_);
    if (report_blocks.empty()) {
        return;
    }

    int num_of_lost_packets = 0;
    int num_of_packets = 0;

    // Compute the lost packets from all report blocks
    for (const auto& rb : report_blocks) {
        auto it = last_report_blocks_.find(rb.source_ssrc);
        if (it != last_report_blocks_.end()) {
            auto packets_delta = rb.extended_highest_sequence_number - it->second.extended_highest_sequence_number;
            num_of_packets += packets_delta;

            auto lost_packets_delta = rb.packets_lost - it->second.packets_lost;
            num_of_lost_packets += lost_packets_delta;
        }
        last_report_blocks_[rb.source_ssrc] = rb;
    }

    if (num_of_packets <= 0 || num_of_packets < num_of_lost_packets) {
        return;
    }

    if (network_controller_) {
        OnNetworkControlUpdate(network_controller_->OnPacketsLost(num_of_lost_packets, num_of_packets, now));
    }
    last_report_block_time_ = now;
}   
    
} // namespace naivertc