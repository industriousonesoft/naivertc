#include "rtc/congestion_control/send_side/goog_cc/send_side_bwe.hpp"
#include "rtc/congestion_control/base/bwe_defines.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr TimeDelta kBweIncreaseInterval = TimeDelta::Millis(1000);
constexpr TimeDelta kStartPhase = TimeDelta::Millis(2000);

} // namespace

SendSideBwe::SendSideBwe(Configuration config)
    : config_(std::move(config)),
      curr_bitrate_(DataRate::Zero()),
      min_configured_bitrate_(kDefaultMinBitrate),
      max_configured_bitrate_(kDefaultMaxBitrate),
      ack_bitrate_(std::nullopt),
      last_rtt_(TimeDelta::Zero()),
      remb_limit_(DataRate::PlusInfinity()),
      use_remb_as_limit_cap_(false),
      delay_based_limit_(DataRate::PlusInfinity()),
      time_first_report_(Timestamp::MinusInfinity()),
      time_last_decrease_(Timestamp::MinusInfinity()),
      loss_report_based_bwe_(LossReportBasedBwe::Configuration()) {
    // Enable loss-feedback-based bandwidth estimator.
    if (config_.enable_loss_feedback_based_control) {
        loss_feedback_based_bwe_.emplace(LossFeedbackBasedBwe(LossFeedbackBasedBwe::Configuration()));
    }
}  

SendSideBwe::~SendSideBwe() = default;

DataRate SendSideBwe::target_bitate() const {
    return Clamp(curr_bitrate_);
}

DataRate SendSideBwe::min_bitate() const {
    return min_configured_bitrate_;
}

DataRate SendSideBwe::EstimatedLinkCapacity() const {
    return linker_capacity_tracker_.estimate();
}

uint8_t SendSideBwe::fraction_loss() const {
    return loss_report_based_bwe_.fraction_loss();
}

TimeDelta SendSideBwe::rtt() const {
    return last_rtt_;
}

void SendSideBwe::OnBitrates(std::optional<DataRate> send_bitrate,
                             DataRate min_bitrate,
                             DataRate max_bitrate,
                             Timestamp report_time) {
    if (send_bitrate) {
        linker_capacity_tracker_.OnStartingBitrate(*send_bitrate);
        OnSendBitrate(*send_bitrate, report_time);
    }
    SetMinMaxBitrate(min_bitrate, max_bitrate);
}

void SendSideBwe::OnSendBitrate(DataRate bitrate,
                                Timestamp report_time) {
    if (bitrate > DataRate::Zero()) {
        // Reset to avoid being caped by the estimate.
        delay_based_limit_ = DataRate::PlusInfinity();
        UpdateTargetBitrate(bitrate, report_time);
        // Clear last sent bitrate history so the new bitrate can
        // be used directly and not capped.
        min_bitrate_history_.clear();
    }
}

void SendSideBwe::OnDelayBasedBitrate(DataRate bitrate,
                                      Timestamp report_time) {
    linker_capacity_tracker_.OnDelayBasedEstimate(bitrate, report_time);
    delay_based_limit_ = bitrate.IsZero() ? DataRate::PlusInfinity()
                                          : bitrate;
    PLOG_VERBOSE_IF(false) << "delay_based_limit=" << delay_based_limit_.bps() << " bps.";
    ApplyLimits(report_time);
}

void SendSideBwe::OnAcknowledgedBitrate(std::optional<DataRate> ack_bitrate,
                                       Timestamp report_time) {
    ack_bitrate_ = ack_bitrate;
    if (ack_bitrate && loss_feedback_based_bwe_) {
        loss_feedback_based_bwe_->OnAcknowledgedBitrate(*ack_bitrate, report_time);
    }
}

void SendSideBwe::OnPropagationRtt(TimeDelta rtt,
                                   Timestamp report_time) {
    rtt_backoff_.OnPropagationRtt(rtt, report_time);
}

void SendSideBwe::OnSentPacket(const SentPacket& sent_packet) {
    rtt_backoff_.OnSentPacket(sent_packet.send_time);
}

void SendSideBwe::OnRemb(DataRate bitrate,
                         Timestamp report_time) {
    PLOG_VERBOSE << "updated REMB=" << bitrate.bps() << " bps.";
    remb_limit_ = bitrate.IsZero() ? DataRate::PlusInfinity()
                                   : bitrate;
    ApplyLimits(report_time);
}

void SendSideBwe::OnPacketsLostReport(int64_t num_packets_lost,
                                      int64_t num_packets,
                                      Timestamp report_time) {
    if (time_first_report_.IsInfinite()) {
        time_first_report_ = report_time;
    }
    // Check sequence number diff and weight loss report.
    if (num_packets > 0) {
        loss_report_based_bwe_.OnPacketsLostReport(num_packets_lost, num_packets, report_time);
        UpdateEstimate(report_time);
    }
}
                
void SendSideBwe::OnRtt(TimeDelta rtt,
                        Timestamp report_time) {
    // Update RTT if we were able to compute an RTT based on this RTCP.
    // FlexFEC doesn't send RTCP SR, which means we won't be able to compute RTT.
    if (rtt > TimeDelta::Zero()) {
        PLOG_VERBOSE << "Rtt: " << last_rtt_.ms() << " ms -> " << rtt.ms() << " ms.";
        last_rtt_ = rtt;
    }
}

void SendSideBwe::IncomingPacketFeedbacks(const TransportPacketsFeedback& report) {
    if (loss_feedback_based_bwe_) {
        loss_feedback_based_bwe_->OnPacketFeedbacks(report.packet_feedbacks, report.receive_time);
    }
}

void SendSideBwe::SetMinMaxBitrate(DataRate min_bitrate,
                                   DataRate max_bitrate) {
    min_configured_bitrate_ = std::max(min_bitrate, kDefaultMinBitrate);
    if (max_configured_bitrate_ > DataRate::Zero() && max_bitrate.IsFinite()) {
        max_configured_bitrate_ = std::max(max_configured_bitrate_, max_bitrate);
    } else {
        max_configured_bitrate_ = kDefaultMaxBitrate;
    }
}

void SendSideBwe::UpdateEstimate(Timestamp report_time) {
    // If the roughly RTT (with backoff) exceed the limit, we assume that 
    // we've been over-using.
    if (rtt_backoff_.CorrectedRtt(report_time) > config_.rtt_limit) {
        // Decrease the bitrate at intervals if the current bitrate is above
        // the floor (the min bitrate as required).
        if (report_time - time_last_decrease_ >= config_.drop_interval &&
            curr_bitrate_ > config_.bandwidth_floor) {
            time_last_decrease_ = report_time;
            DataRate new_bitrate = std::max(curr_bitrate_ * config_.drop_factor,
                                            config_.bandwidth_floor);
            linker_capacity_tracker_.OnRttBackoffEstimate(new_bitrate, report_time);
            UpdateTargetBitrate(new_bitrate, report_time);
        }
        return;
    }

    // We choose to trust the REMB and/or delay-based estimate during the start phase (2s)
    // if we haven't had any packet loss reported, to allow startup bitrate probing.
    if (fraction_loss() == 0 && IsInStartPhase(report_time)) {
        DataRate new_bitrate = curr_bitrate_;

        if (remb_limit_.IsFinite()) {
            // TODO: We should not allow the new_bitrate to be larger than the
            // receiver limit here.
            new_bitrate = std::max(remb_limit_, new_bitrate);
        }
        if (delay_based_limit_.IsFinite()) {
            new_bitrate = std::max(delay_based_limit_, new_bitrate);
        }
        if (loss_feedback_based_bwe_) {
            loss_feedback_based_bwe_->SetInitialBitrate(new_bitrate);
        }

        if (new_bitrate != curr_bitrate_) {
            min_bitrate_history_.clear();
            if (loss_feedback_based_bwe_) {
                min_bitrate_history_.push_back({report_time, new_bitrate});
            } else {
                min_bitrate_history_.push_back({report_time, curr_bitrate_});
            }
            UpdateTargetBitrate(new_bitrate, report_time);
            return;
        }
    } 
    UpdateMinHistory(curr_bitrate_, report_time);

    auto min_bitrate = min_bitrate_history_.front().second;
    // The loss estimat based packet feedbacks has higher priority.
    if (loss_feedback_based_bwe_->InUse()) {
        // NOTE: |loss_feedback_based_bwe_|中的降码操作是基于acknowledged bitrate
        // 而非当前码率|curr_bitrate_|，因此即使|state = DECREASE|也不需要更新time_last_decrease_。
        auto [new_bitrate, state] = loss_feedback_based_bwe_->Estimate(min_bitrate,
                                                                       delay_based_limit_,
                                                                       last_rtt_,
                                                                       report_time);
        UpdateTargetBitrate(new_bitrate, report_time);
        return;
    }

    // 基于丢包信息调整当前码率值
    auto [new_bitrate, state] = loss_report_based_bwe_.Estimate(min_bitrate, 
                                                                curr_bitrate_,
                                                                last_rtt_,
                                                                report_time);
    UpdateTargetBitrate(new_bitrate, report_time);
    // NOTE: |rtt_backoff_|和|loss_report_based_bwe_|的降码逻辑都是基于
    // 当前码率|curr_bitrate_|，因此当|loss_report_based_bwe_|降低码率时
    // 需更新相应的时间。
    if (state == RateControlState::DECREASE) {
        time_last_decrease_ = report_time;
    }

}

// Private methods
DataRate SendSideBwe::Clamp(DataRate bitrate) const {
    // Using REMB as limit cap.
    if (use_remb_as_limit_cap_ && remb_limit_.IsFinite()) {
        bitrate = std::min(bitrate, remb_limit_);
    }
    // Limit the bitrate below the max configured bitrate.
    return std::min(bitrate, max_configured_bitrate_);
}

DataRate SendSideBwe::GetUpperLimit() const {
    // The upper limit of bitrate is based on delay based
    // limit.
    return Clamp(delay_based_limit_);
}

void SendSideBwe::UpdateTargetBitrate(DataRate new_bitrate, 
                                      Timestamp at_time) {
    new_bitrate = std::min(new_bitrate, GetUpperLimit());
    if (new_bitrate < min_configured_bitrate_) {
        PLOG_WARNING << "The estimated bitrate " << new_bitrate.bps() << " bps "
                     << "is below the configured min bitrate " << min_configured_bitrate_.bps() << " bps.";
        new_bitrate = min_configured_bitrate_;
    }
    if (curr_bitrate_ != new_bitrate) {
        PLOG_INFO_IF(false) << "Update bitrate from " << curr_bitrate_.bps()
                           << " bps to " << new_bitrate.bps() << " bps.";
        curr_bitrate_ = new_bitrate;
    }
    // Make sure that we have measured a throughput before updating the link capacity.
    if (ack_bitrate_) {
        // Use the smaller as the linker capacity estimate.
        linker_capacity_tracker_.OnBitrateUpdated(std::min(*ack_bitrate_, curr_bitrate_), at_time);
    }
}

void SendSideBwe::ApplyLimits(Timestamp report_time) {
    UpdateTargetBitrate(curr_bitrate_, report_time);
}

bool SendSideBwe::IsInStartPhase(Timestamp report_time) const {
    return time_first_report_.IsInfinite() ||
           report_time - time_first_report_ < kStartPhase;
}

void SendSideBwe::UpdateMinHistory(DataRate bitrate, Timestamp report_time) {
    // Remove old data points from history.
    // Since history precision is in ms, add one so it is able to 
    // increase bitrate if it is off by as little as 0.5ms.
    const TimeDelta percision_correction = TimeDelta::Millis(1);
    while (!min_bitrate_history_.empty() && 
           report_time - min_bitrate_history_.front().first + percision_correction > kBweIncreaseInterval) {
        min_bitrate_history_.pop_front();
    }

    // Typical minimum sliding-window algorithm:
    // Pop values higher than current birate before pushint it.
    while (!min_bitrate_history_.empty() && 
           bitrate <= min_bitrate_history_.back().second) {
        min_bitrate_history_.pop_back();
    }

    min_bitrate_history_.push_back({report_time, bitrate});
}

} // namespace naivertc
