#include "rtc/call/rtp_send_controller.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/transport_feedback.hpp"
#include "rtc/congestion_control/send_side/goog_cc/goog_cc_network_controller.hpp"
#include "rtc/congestion_control/components/congestion_control_handler.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

// Goog-CC process interval: 25ms
constexpr TimeDelta kUpdateInterval = TimeDelta::Millis(25);

std::unique_ptr<TaskQueuePacedSender> CreatePacer(Clock* clock, TaskQueueImpl* pacing_queue) {
    TaskQueuePacedSender::Configuration config;
    config.clock = clock;
    // TODO: Sets packet sende, pacing_settings and probing_settings.
    // config.packet_sender = nullptr;
    // config.pacing_settings
    // config.probing_settings
    return std::make_unique<TaskQueuePacedSender>(config, pacing_queue);
}

} // namespace

RtpSendController::RtpSendController(const Configuration& config) 
    : clock_(config.clock),
      add_pacing_to_cwin_(config.add_pacing_to_cwin),
      task_queue_("RtpSendController.worker.queue"),
      pacing_queue_("RtpSendController.pacing.queue"),
      is_started_(false),
      network_available_(false),
      pacer_(CreatePacer(clock_, pacing_queue_.Get())),
      last_report_block_time_(clock_->CurrentTime()) {
    assert(clock_ != nullptr);

    task_queue_.Post([this, config](){
        network_config_.clock = clock_;
        // Initial target bitrate settings.
        network_config_.constraints.min_bitrate = config.min_bitrate;
        network_config_.constraints.max_bitrate = config.max_bitrate;
        network_config_.constraints.starting_bitrate = config.starting_bitrate;

        // Initial pacer.
        pacer_->SetPacingBitrates(config.starting_bitrate, DataRate::Zero());
    });

}

RtpSendController::~RtpSendController() {
    Clear();
}

void RtpSendController::Clear() {
    if (repeating_update_task_) {
        repeating_update_task_->Stop();
    }
    task_queue_.Post([this](){
        // Reset callbacks
        target_transfer_bitrate_update_callback_ = nullptr;
    });
}

void RtpSendController::EnsureStarted() {
    task_queue_.Post([this](){
        if (is_started_) {
            return;
        }
        is_started_ = true;

        pacer_->EnsureStarted();
    });
}

void RtpSendController::OnNetworkAvailability(bool network_available) {
    NetworkAvailability msg;
    msg.network_available = network_available;
    msg.at_time = clock_->CurrentTime();
    task_queue_.Post([this, msg=std::move(msg)](){
        if (network_available_ == msg.network_available) {
            return;
        }

        // Congestion control handler
        control_handler_ = std::make_unique<CongestionControlHandler>();

        network_available_ = msg.network_available;
        msg.network_available ? pacer_->Resume() : pacer_->Pause();
        pacer_->OnInflightBytes(0);

        if (network_controller_) {
            PostUpdates(network_controller_->OnNetworkAvailability(msg));
        } else {
            MaybeCreateNetworkController();
        }
    });
}

/*
void RtpSendController::OnReceivedPacket(const ReceivedPacket& recv_packet) {
    task_queue_.Post([this, recv_packet](){
        if (network_controller_) {
            PostUpdates(network_controller_->OnReceivedPacket(recv_packet));
        }
    });
}
*/

void RtpSendController::OnReceivedEstimatedBitrateBps(uint32_t bitrate_bps) {
    DataRate remb = DataRate::BitsPerSec(bitrate_bps);
    Timestamp at_time = clock_->CurrentTime();
    task_queue_.Post([this, remb, at_time](){
        if (network_controller_) {
            PostUpdates(network_controller_->OnRembUpdated(remb, at_time));
        }
    });
}

void RtpSendController::OnAddPacket(const RtpPacketSendInfo& packet_info) {
    Timestamp at_time = clock_->CurrentTime();
    task_queue_.Post([this, packet_info, at_time](){
        transport_statistician_.AddPacket(packet_info, /*TODO: transport_overhead_bytes=*/0, at_time);
    });
}

void RtpSendController::OnSentPacket(const RtpSentPacket& sent_packet) {
    task_queue_.Post([this, sent_packet](){
        auto sent_msg = transport_statistician_.ProcessSentPacket(sent_packet);
        if (sent_msg) {
            // Only update outstanding data in pacer if:
            // 1. Packet feedback is used.
            // 2. The packet has not yet received an acknowledgement.
            // 3. It's not a retransmission of an earlier packet.
            pacer_->OnInflightBytes(transport_statistician_.GetInFlightBytes());
            if (network_controller_) {
                PostUpdates(network_controller_->OnSentPacket(*sent_msg));
            }
        }
    });
}

void RtpSendController::OnTransportFeedback(const rtcp::TransportFeedback& feedback) {
    Timestamp at_time = clock_->CurrentTime();
    task_queue_.Post([this, feedback, at_time](){
        auto feedback_msg = transport_statistician_.ProcessTransportFeedback(feedback, at_time);
        if (feedback_msg) {
            if (network_controller_) {
                PostUpdates(network_controller_->OnTransportPacketsFeedback(*feedback_msg));
            }
            
            // Only update outstanding data in pacer if any packet is first
            // time acked.
            pacer_->OnInflightBytes(transport_statistician_.GetInFlightBytes());
        }
    });
}

void RtpSendController::OnReceivedRtcpReceiveReport(const std::vector<RtcpReportBlock>& report_blocks,
                                                    int64_t rtt_ms) {
    auto at_time = clock_->CurrentTime();
    task_queue_.Post([this, report_blocks, at_time](){
        HandleRtcpReportBlocks(report_blocks, at_time);
    });

    task_queue_.Post([this, rtt_ms, at_time](){
        if (network_controller_ && rtt_ms > 0) {
            TimeDelta rtt = TimeDelta::Millis(rtt_ms);
            PostUpdates(network_controller_->OnRttUpdated(rtt, at_time));
        }
    });
}

void RtpSendController::OnTargetTransferBitrateUpdated(TargetTransferBitrateUpdateCallback&& callback) {
    task_queue_.Post([this, callback=std::move(callback)]{
        target_transfer_bitrate_update_callback_ = std::move(callback);
    });
}

// Private methods
void RtpSendController::MaybeCreateNetworkController() {
    RTC_RUN_ON(&task_queue_);
    if (!network_available_) {
        return;
    }

    // GoogCcNetworkController
    network_config_.constraints.at_time = clock_->CurrentTime();
    network_controller_ = std::make_unique<GoogCcNetworkController>(network_config_, /*packet_feedback_only=*/false);

    UpdatePeriodically();
    StartPeriodicTasks();
}

void RtpSendController::HandleRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                               Timestamp receive_time) {
    RTC_RUN_ON(&task_queue_);
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
        PostUpdates(network_controller_->OnTransportLostReport(loss_report));
    }
    last_report_block_time_ = receive_time;
} 

void RtpSendController::StartPeriodicTasks() {
    if (repeating_update_task_) {
        repeating_update_task_->Stop();
    }
    if (kUpdateInterval.IsFinite()) {
        repeating_update_task_ = RepeatingTask::DelayedStart(clock_, task_queue_.Get(), kUpdateInterval, [this](){
            UpdatePeriodically();
            return kUpdateInterval;
        });
    }
}

void RtpSendController::UpdatePeriodically() {
    // Update pacer queue time.
    control_handler_->OnPacerExpectedQueueTime(pacer_->GetStats().expected_queue_time);
    // Network controller update periodically.
    PeriodicUpdate msg;
    msg.at_time = clock_->CurrentTime();
    if (add_pacing_to_cwin_) {
        msg.pacer_queue_size = pacer_->GetStats().queue_size;
    }
    PostUpdates(network_controller_->OnPeriodicUpdate(msg));
}


void RtpSendController::PostUpdates(NetworkControlUpdate update) {
    RTC_RUN_ON(&task_queue_);
    if (update.congestion_window) {
        pacer_->SetCongestionWindow(*update.congestion_window);
    }
    if (update.pacer_config) {
        pacer_->SetPacingBitrates(update.pacer_config->pacing_bitrate, update.pacer_config->padding_bitrate);
    }
    for (const auto& probe : update.probe_cluster_configs) {
        pacer_->AddProbeCluster(probe.id, probe.target_bitrate);
    }
    if (update.target_bitrate) {
        control_handler_->OnTargetTransferBitrate(*update.target_bitrate);
    }
    MaybeUpdateControlState();
}

void RtpSendController::MaybeUpdateControlState() {
    RTC_RUN_ON(&task_queue_);
    auto update = control_handler_->GetUpdate();
    if (!update) {
        return;
    }
    if (target_transfer_bitrate_update_callback_) {
        target_transfer_bitrate_update_callback_(*update);
    }
}

} // namespace naivertc