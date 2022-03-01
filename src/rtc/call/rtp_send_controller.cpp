#include "rtc/call/rtp_send_controller.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/transport_feedback.hpp"
#include "rtc/congestion_control/controllers/goog_cc/goog_cc_network_controller.hpp"

namespace naivertc {
namespace {

// Goog-CC process interval: 25ms
constexpr TimeDelta kUpdateInterval = TimeDelta::Millis(25);

// Customize goog_cc 
GoogCcNetworkController::Configuration BuildGoogCCConfig(Clock* clock) {
    GoogCcNetworkController::Configuration config;
    config.clock = clock;
    return config;
}
    
} // namespace

RtpSendController::RtpSendController(Clock* clock) 
    : clock_(clock),
      worker_queue_("RtpSendController.worker.queue"),
      update_interval_(kUpdateInterval),
      network_controller_(std::make_unique<GoogCcNetworkController>(BuildGoogCCConfig(clock_))),
      last_report_block_time_(clock_->CurrentTime()) {
    assert(clock != nullptr);
    if (update_interval_.IsFinite()) {
        controller_task_ = RepeatingTask::DelayedStart(clock_, worker_queue_.Get(), update_interval_, [this](){
            UpdatePeriodically();
            return update_interval_;
        });
    }
}

RtpSendController::~RtpSendController() {
    worker_queue_.Invoke<void>([this](){
        if (controller_task_ && controller_task_->Running()) {
            controller_task_->Stop();
        }
        controller_task_.reset();
    });
}

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

void RtpSendController::OnAddPacket(const RtpPacketSendInfo& packet_info) {
    Timestamp receive_time = clock_->CurrentTime();
    worker_queue_.Post([this, packet_info, receive_time](){
        transport_statistician_.AddPacket(packet_info, /*TODO: transport_overhead_bytes=*/0, receive_time);
    });
}

void RtpSendController::OnSentPacket(const RtpSentPacket& sent_packet) {
    worker_queue_.Post([this, sent_packet](){
        auto sent_msg = transport_statistician_.ProcessSentPacket(sent_packet);
        if (sent_msg && network_controller_) {
            OnNetworkControlUpdate(network_controller_->OnSentPacket(*sent_msg));
        }
    });
}

void RtpSendController::OnTransportFeedback(const rtcp::TransportFeedback& feedback) {
    Timestamp receive_time = clock_->CurrentTime();
    worker_queue_.Post([this, feedback, receive_time](){
        auto feedback_msg = transport_statistician_.ProcessTransportFeedback(feedback, receive_time);
        if (feedback_msg && network_controller_) {
            OnNetworkControlUpdate(network_controller_->OnTransportPacketsFeedback(*feedback_msg));
        }
    });
}

void RtpSendController::OnReceivedRtcpReceiveReport(const std::vector<RtcpReportBlock>& report_blocks,
                                                    int64_t rtt_ms) {
    Timestamp receive_time = clock_->CurrentTime();
    worker_queue_.Post([this, report_blocks, receive_time](){
        HandleRtcpReportBlocks(report_blocks, receive_time);
    });

    worker_queue_.Post([this, rtt_ms, receive_time](){
        if (network_controller_ && rtt_ms > 0) {
            TimeDelta rtt = TimeDelta::Millis(rtt_ms);
            OnNetworkControlUpdate(network_controller_->OnRttUpdated(rtt, receive_time));
        }
    });
}

// Private methods
void RtpSendController::OnNetworkControlUpdate(NetworkControlUpdate update) {
    RTC_RUN_ON(&worker_queue_);

}

void RtpSendController::HandleRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                               Timestamp receive_time) {
    RTC_RUN_ON(&worker_queue_);
    if (report_blocks.empty()) {
        return;
    }

    int num_packets_lost = 0;
    int num_packets = 0;

    // Compute the lost packets from all report blocks
    for (const auto& rb : report_blocks) {
        auto it = last_report_blocks_.find(rb.source_ssrc);
        if (it != last_report_blocks_.end()) {
            auto packets_delta = rb.extended_highest_sequence_number - it->second.extended_highest_sequence_number;
            num_packets += packets_delta;

            auto lost_packets_delta = rb.packets_lost - it->second.packets_lost;
            num_packets_lost += lost_packets_delta;
        }
        last_report_blocks_[rb.source_ssrc] = rb;
    }

    if (num_packets <= 0 || num_packets < num_packets_lost) {
        return;
    }

    if (network_controller_) {
        TransportLossReport loss_report;
        loss_report.num_packets_lost = num_packets_lost;
        loss_report.num_packets = num_packets;
        loss_report.receive_time = receive_time;
        // Update loss report.
        OnNetworkControlUpdate(network_controller_->OnTransportLostReport(loss_report));
    }
    last_report_block_time_ = receive_time;
} 

void RtpSendController::UpdatePeriodically() {
    PeriodicUpdate msg;
    msg.at_time = clock_->CurrentTime();
    // TODO: add pacing to cwin
    OnNetworkControlUpdate(network_controller_->OnPeriodicUpdate(msg));
}
    
} // namespace naivertc