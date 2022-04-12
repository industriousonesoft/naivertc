#include "rtc/congestion_control/components/congestion_control_handler.hpp"
#include "rtc/congestion_control/pacing/pacing_controller.hpp"

#include <plog/Log.h>

namespace naivertc {

CongestionControlHandler::CongestionControlHandler(bool enable_pacer_emergency_stop) 
    : enable_pacer_emergency_stop_(enable_pacer_emergency_stop) {
    sequence_checker_.Detach();
}

CongestionControlHandler::~CongestionControlHandler() = default;

void CongestionControlHandler::OnPacerExpectedQueueTime(TimeDelta expected_queue_time) {
    RTC_RUN_ON(&sequence_checker_);
    pacer_expected_queue_time_ = expected_queue_time;
}

void CongestionControlHandler::OnTargetTransferBitrate(TargetTransferBitrate target_bitrate) {
    RTC_RUN_ON(&sequence_checker_);
    assert(target_bitrate.at_time.IsFinite());
    last_incoming_ = target_bitrate;
}

void CongestionControlHandler::OnNetworkAvailability(bool network_available) {
    RTC_RUN_ON(&sequence_checker_);
    network_available_ = network_available;
}

std::optional<TargetTransferBitrate> CongestionControlHandler::GetUpdate() {
    RTC_RUN_ON(&sequence_checker_);
    if (!last_incoming_) {
        return std::nullopt;
    }
    TargetTransferBitrate new_outgoing = *last_incoming_;
    bool pause_encoding = false;
    if (!network_available_) {
        // Pause the encoder when the network is unavailable.
        pause_encoding = true;
    } else if (enable_pacer_emergency_stop_ && 
               pacer_expected_queue_time_ > PacingController::kMaxExpectedQueueTime) {
        // Pause the encoder when the pacer is congested.
        pause_encoding = true;
    }
    // Set the target bitrate to zero to pause the encoder.
    if (pause_encoding) {
        new_outgoing.target_bitrate = DataRate::Zero();
    }
    // Check if the report was updated or not.
    if (BelongsToNewReport(new_outgoing)) {
        last_reported_ = new_outgoing;
        return new_outgoing;
    } else {
        return std::nullopt;
    }
}

// Private methods
bool CongestionControlHandler::BelongsToNewReport(const TargetTransferBitrate& new_outgoing) const {
    // Check if it's the first report.
    if (!last_reported_) {
        return true;
    }
    // Check if the target bitrate has changed.
    if (last_reported_->target_bitrate != new_outgoing.target_bitrate) {
        PLOG_INFO_IF(false) 
            << "Bitrate estimate changed: " << last_reported_->target_bitrate.bps()
            << " bps -> " << new_outgoing.target_bitrate.bps() << " bps.";
        return true;
    }
    // Check if the network estimate has changed.
    if (!new_outgoing.target_bitrate.IsZero()) {
        if (last_reported_->network_estimate.loss_rate_ratio != new_outgoing.network_estimate.loss_rate_ratio ||
            last_reported_->network_estimate.rtt != new_outgoing.network_estimate.rtt) {
            PLOG_INFO_IF(true)
                << "Network estimate state changed, loss_rate_ratio: " << last_reported_->network_estimate.loss_rate_ratio
                << " -> " << new_outgoing.network_estimate.loss_rate_ratio
                << ", rtt: " << last_reported_->network_estimate.rtt.ms() 
                << "ms -> " << new_outgoing.network_estimate.rtt.ms() << " ms.";
            return true;
        }
    }
    // Not count as a new report. 
    return false;
}
    
} // namespace naivertc