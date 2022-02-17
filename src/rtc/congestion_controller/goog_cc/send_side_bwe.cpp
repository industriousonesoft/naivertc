#include "rtc/congestion_controller/goog_cc/send_side_bwe.hpp"
#include "rtc/congestion_controller/goog_cc/bwe_defines.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr TimeDelta kBweIncreaseInterval = TimeDelta::Millis(1000);
constexpr TimeDelta kBweDecreaseInterval = TimeDelta::Millis(300);
constexpr TimeDelta kStartPhase = TimeDelta::Millis(2000);
constexpr TimeDelta kBweConverganceTime = TimeDelta::Millis(20000);
constexpr int kLimitNumPackets = 20;
constexpr DataRate kDefaultMaxBitrate = DataRate::BitsPerSec(1000000000);
// Expecting that RTCP feedback is sent uniformly within [0.5, 1.5]s intervals.
constexpr TimeDelta kMaxRtcpFeedbackInterval = TimeDelta::Millis(5000);

constexpr float kDefaultLowLossThreshold = 0.02f;
constexpr float kDefaultHighLossThreshold = 0.1f;
constexpr DataRate kDefaultBitrateThreshold = DataRate::Zero();

struct UmaRampUpMetric {
    const char* metric_name;
    int bitrate_kbps;
};

const UmaRampUpMetric kUmaRampupMetrics[] = {
    {"NaivrRTC.BWE.RampUpTimeTo500kbpsInMs", 500},
    {"NaivrRTC.BWE.RampUpTimeTo1000kbpsInMs", 1000},
    {"NaivrRTC.BWE.RampUpTimeTo2000kbpsInMs", 2000}};
const size_t kNumUmaRampupMetrics = sizeof(kUmaRampupMetrics) / sizeof(kUmaRampupMetrics[0]);
    
} // namespace

SendSideBwe::SendSideBwe(Configuration config)
    : config_(std::move(config)),
      rtt_backoff_(),
      linker_capacity_tracker_(),
      accumulated_lost_packets_(0),
      accumulated_packets_(0),
      curr_bitrate_(DataRate::Zero()),
      min_configured_bitrate_(kMinBitrate),
      max_configured_bitrate_(kDefaultMaxBitrate),
      ack_bitrate_(std::nullopt),
      has_decreased_since_last_fraction_loss_(false),
      time_last_fraction_loss_update_(Timestamp::MinusInfinity()),
      last_fraction_loss_(0),
      last_rtt_(TimeDelta::Zero()),
      remb_limit_(DataRate::PlusInfinity()),
      use_remb_limit_cpas_only_(true),
      delay_based_limit_(DataRate::PlusInfinity()),
      time_last_decrease_(Timestamp::MinusInfinity()),
      time_first_report_(Timestamp::MinusInfinity()),
      initially_loss_packets_(0),
      bitrate_at_start_(DataRate::Zero()),
      uma_update_state_(NO_UPDATE),
      uma_rtt_state_(NO_UPDATE),
      rampup_uma_states_updated_(kNumUmaRampupMetrics, false),
      low_loss_threshold_(kDefaultLowLossThreshold),
      high_loss_threshold_(kDefaultHighLossThreshold),
      bitrate_threshold_(kDefaultBitrateThreshold),
      loss_based_bwe_(std::make_optional<LossBasedBwe>(LossBasedBwe::Configuration())) {}

SendSideBwe::~SendSideBwe() = default;

DataRate SendSideBwe::target_bitate() const {
    return curr_bitrate_;
}

DataRate SendSideBwe::min_bitate() const {
    return min_configured_bitrate_;
}

DataRate SendSideBwe::EstimatedLinkCapacity() const {
    return linker_capacity_tracker_.estimate();
}

uint8_t SendSideBwe::fraction_loss() const {
    return last_fraction_loss_;
}

TimeDelta SendSideBwe::rtt() const {
    return last_rtt_;
}

void SendSideBwe::OnBitrates(std::optional<DataRate> send_bitrate,
                             DataRate min_bitrate,
                             DataRate max_bitrate,
                             Timestamp at_time) {
    if (send_bitrate) {
        linker_capacity_tracker_.OnStartingBitrate(*send_bitrate);
        OnSendBitrate(*send_bitrate, at_time);
    }
    SetBitrateBoundary(min_bitrate, max_bitrate);
}

void SendSideBwe::OnSendBitrate(DataRate bitrate,
                                Timestamp at_time) {
    if (bitrate > DataRate::Zero()) {
        // Reset to avoid being caped by the estimate.
        delay_based_limit_ = DataRate::PlusInfinity();
        UpdateTargetBitrate(bitrate, at_time);
        // Clear last sent bitrate history so the new bitrate can
        // be used directly and not capped.
        min_bitrate_history_.clear();
    }
}

void SendSideBwe::OnDelayBasedBitrate(DataRate bitrate,
                                      Timestamp at_time) {
    linker_capacity_tracker_.OnDelayBasedEstimate(bitrate, at_time);
    delay_based_limit_ = bitrate.IsZero() ? DataRate::PlusInfinity()
                                          : bitrate;
    ApplyLimits(at_time);
}

void SendSideBwe::OnAcknowledgeBitrate(std::optional<DataRate> ack_bitrate,
                                       Timestamp at_time) {
    ack_bitrate_ = ack_bitrate;
    if (ack_bitrate && loss_based_bwe_) {
        loss_based_bwe_->OnAcknowledgedBitrate(*ack_bitrate, at_time);
    }
}

void SendSideBwe::OnPropagationRtt(TimeDelta rtt,
                                   Timestamp at_time) {
    rtt_backoff_.Update(rtt, at_time);
}

void SendSideBwe::OnSentPacket(const SentPacket& sent_packet) {
    rtt_backoff_.OnSentPacket(sent_packet);
}

void SendSideBwe::OnRemb(DataRate bitrate,
                         Timestamp at_time) {
    remb_limit_ = bitrate.IsZero() ? DataRate::PlusInfinity()
                                   : bitrate;
    ApplyLimits(at_time);
}

void SendSideBwe::OnPacketsLost(int64_t num_of_packets_lost,
                                int64_t num_of_packets,
                                Timestamp at_time) {
    if (time_first_report_.IsInfinite()) {
        time_first_report_ = at_time;
    }

    // Check sequence number diff and weight loss report.
    if (num_of_packets > 0) {
        accumulated_packets_ += num_of_packets;
        accumulated_lost_packets_ += num_of_packets_lost;

        // Don't generate a loss rate until it can be based on enough packets.
        if (accumulated_packets_ < kLimitNumPackets) {
            return;
        }
        int64_t lost_q8 = accumulated_lost_packets_ << 8;
        last_fraction_loss_ = std::min<uint8_t>(lost_q8 / accumulated_packets_, 255);

        // Reset accumulator.
        accumulated_lost_packets_ = 0;
        accumulated_packets_ = 0;
        time_last_fraction_loss_update_ = at_time;
        has_decreased_since_last_fraction_loss_ = false;
        UpdateEstimate(at_time);
    }
    UpdateUmaStats(num_of_packets_lost, at_time);
}
                
void SendSideBwe::OnRtt(TimeDelta rtt,
                        Timestamp at_time) {
    // Update RTT if we were able to compute an RTT based on this RTCP.
    // FlexFEC doesn't send RTCP SR, which means we won't be able to compute RTT.
    if (rtt > TimeDelta::Zero()) {
        last_rtt_ = rtt;
    }
    if (IsInStartPhase(at_time) && uma_rtt_state_ == NO_UPDATE) {
        uma_rtt_state_ = DONE;
    }
}

void SendSideBwe::IncomingPacketFeedbacks(const TransportPacketsFeedback& report) {
    if (loss_based_bwe_) {
        loss_based_bwe_->IncomingFeedbacks(report.packet_feedbacks, report.feedback_time);
    }
}

void SendSideBwe::SetBitrateBoundary(DataRate min_bitrate,
                                     DataRate max_bitrate) {
    min_configured_bitrate_ = std::max(min_bitrate, kMinBitrate);
    if (max_configured_bitrate_ > DataRate::Zero() && max_bitrate.IsFinite()) {
        max_configured_bitrate_ = std::max(max_configured_bitrate_, max_bitrate);
    } else {
        max_configured_bitrate_ = kDefaultMaxBitrate;
    }
}

void SendSideBwe::UpdateEstimate(Timestamp at_time) {
    // If the RTT that is esitmated right now is upper the limit,
    // which means that congestion has detected.
    // And we will decrease the bitrate if we could.
    if (rtt_backoff_.CorrectedRtt() > config_.rtt_limit) {
        // Not do drop too aften and make sure there has space to drop.
        if (at_time - time_last_decrease_ >= config_.drop_interval &&
            curr_bitrate_ > config_.bandwidth_floor) {
            time_last_decrease_ = at_time;
            DataRate new_bitrate = std::max(curr_bitrate_ * config_.drop_factor,
                                            config_.bandwidth_floor);
            linker_capacity_tracker_.OnRttBasedEstimate(new_bitrate, at_time);
            UpdateTargetBitrate(new_bitrate, at_time);
        }
        return;
    }

    // We choose to trust the REMB and/or delay-based estimate during the start phase (2s)
    // if we haven't had any packet loss reported, to allow startup bitrate probing.
    if (last_fraction_loss_ == 0 && IsInStartPhase(at_time)) {
        DataRate new_bitrate = curr_bitrate_;

        if (remb_limit_.IsFinite()) {
            // FIXME: Why the new bitrate should not be larger than the REMB?
            new_bitrate = remb_limit_;
        }
        if (delay_based_limit_.IsFinite()) {
            new_bitrate = std::max(delay_based_limit_, new_bitrate);
        }
        if (loss_based_bwe_) {
            loss_based_bwe_->SetInitialBitrate(new_bitrate);
        }

        if (new_bitrate != curr_bitrate_) {
            min_bitrate_history_.clear();
            if (loss_based_bwe_) {
                min_bitrate_history_.push_back({at_time, new_bitrate});
            } else {
                min_bitrate_history_.push_back({at_time, curr_bitrate_});
            }
            UpdateTargetBitrate(new_bitrate, at_time);
            return;
        }
    } 
    UpdateMinHistory(curr_bitrate_, at_time);

    // No loss information updated yet.
    if (time_last_fraction_loss_update_.IsInfinite()) {
        return;
    }
    
    if (loss_based_bwe_) {
        auto esimate = loss_based_bwe_->Estimate(min_bitrate_history_.front().second,
                                                 delay_based_limit_,
                                                 last_rtt_,
                                                 at_time);
        if (esimate) {
            UpdateTargetBitrate(*esimate, at_time);
            return;
        }
    }

    TimeDelta elapsed_time = at_time - time_last_fraction_loss_update_;
    // The loss information has updated since last time is still valid.
    if (elapsed_time < 1.2 * kMaxRtcpFeedbackInterval) {
        // We only care about loss above a given bitrate threshold.
        float loss_ratio = last_fraction_loss_ / 256.0f;
        // We only make decisions based on loss when the bitrate is above a
        // threshold. This is a crude way of handling loss which is uncorrelated
        // to congestion.
        if (curr_bitrate_ < bitrate_threshold_ || loss_ratio <= low_loss_threshold_) {
            // Loss < 2%: Increase rate by 8% of the min bitrate in the last
            // kBweIncreaseInterval.
            // Note that by remembering the bitrate over the last second one can
            // rampup up one second faster than if only allowed to start ramping
            // at 8% per second rate now. E.g.:
            //   If sending a constant 100kbps it can rampup immediately to 108kbps
            //   whenever a receiver report is received with lower packet loss.
            //   If instead one would do: current_bitrate_ *= 1.08^(delta time),
            //   it would take over one second since the lower packet loss to achieve
            //   108kbps.
            DataRate new_bitrate = DataRate::BitsPerSec(1.08 * min_bitrate_history_.front().second.bps() + 0.5);

            // Add 1 kbps extra, just to make sure that we do not get stuck
            // (gives a little extra increase at low rates, negligible at higher
            // rates).
            new_bitrate += DataRate::KilobitsPerSec(1);
            UpdateTargetBitrate(new_bitrate, at_time);
            return;
        } else if (curr_bitrate_ > bitrate_threshold_) {
            if (loss_ratio <= high_loss_threshold_) {
                // Loss ratio between 2% ~ 10%, do nothing.
            } else {
                // Loss ratio > 10%: Limit the rate decreases to once a kBweDecreaseInterval + RTT.
                if (!has_decreased_since_last_fraction_loss_ &&
                    (at_time-time_last_decrease_) > (kBweDecreaseInterval + last_rtt_)) {
                    time_last_decrease_ = at_time;

                    // Reduce bitrate: new_bitrate = curr_bitrate * (1 - 0.5 * loss_ratio)
                    DataRate new_bitrate = DataRate::BitsPerSec(curr_bitrate_.bps() * static_cast<double>((512 - last_fraction_loss_) / 512.0));
                    has_decreased_since_last_fraction_loss_ = true;
                    UpdateTargetBitrate(new_bitrate, at_time);
                    return;
                }
            }
        }
    }   
}

// Private methods
DataRate SendSideBwe::Clamp(DataRate bitrate) const {
    DataRate upper_limit = remb_limit_;
    if (!use_remb_limit_cpas_only_) {
        // The delay based limit.
        upper_limit = std::min(delay_based_limit_, remb_limit_);
        // The configured max limit.
        upper_limit = std::min(upper_limit, max_configured_bitrate_);
    }
    if (bitrate > upper_limit) {
        bitrate = upper_limit;
    }
    if (bitrate < min_configured_bitrate_) {
        PLOG_WARNING << "The estimated bitrate " << bitrate.bps() << " bps "
                     << "is below the configured min bitrate " << min_configured_bitrate_.bps() << " bps.";
        bitrate = min_configured_bitrate_;
    }
    return bitrate;
}

void SendSideBwe::UpdateTargetBitrate(DataRate bitrate, 
                                      Timestamp at_time) {
    curr_bitrate_ = Clamp(bitrate);
    // Make sure that we have measured a throughput before updating the link capacity.
    if (ack_bitrate_) {
        linker_capacity_tracker_.Update(std::min(*ack_bitrate_, curr_bitrate_), at_time);
    }
}

void SendSideBwe::ApplyLimits(Timestamp at_time) {
    UpdateTargetBitrate(curr_bitrate_, at_time);
}

bool SendSideBwe::IsInStartPhase(Timestamp at_time) const {
    return time_first_report_.IsInfinite() ||
           at_time - time_first_report_ < kStartPhase;
}

void SendSideBwe::UpdateMinHistory(DataRate bitrate, Timestamp at_time) {
    // Remove old data points from history.
    // Since history precision is in ms, add one so it is able to 
    // increase bitrate if it is off by as little as 0.5ms.
    const TimeDelta percision_correction = TimeDelta::Millis(1);
    while (!min_bitrate_history_.empty() && 
           at_time - min_bitrate_history_.front().first + percision_correction > kBweIncreaseInterval) {
        min_bitrate_history_.pop_front();
    }

    // Typical minimum sliding-window algorithm:
    // Pop values higher than current birate before pushint it.
    while (!min_bitrate_history_.empty() && 
           bitrate <= min_bitrate_history_.back().second) {
        min_bitrate_history_.pop_back();
    }

    min_bitrate_history_.push_back({at_time, bitrate});

}

void SendSideBwe::UpdateUmaStats(int packet_lost, Timestamp at_time) {
    // FIXME: |curr_bitrate_| should be finite, but it's not?
    DataRate bitrate = curr_bitrate_;
    for (size_t i = 0; i < kNumUmaRampupMetrics; ++i) {
        if (!rampup_uma_states_updated_[i] &&
            bitrate.kbps() >= kUmaRampupMetrics[i].bitrate_kbps) {
            rampup_uma_states_updated_[i] = true;
        }
    }

    if (IsInStartPhase(at_time)) {
        initially_loss_packets_ += packet_lost;
    } else if (uma_update_state_ == UmaState::NO_UPDATE) {
        uma_update_state_ = UmaState::FIRST_DONE;
        bitrate_at_start_ = bitrate;
    } else if (uma_update_state_ == UmaState::FIRST_DONE &&
               at_time - time_first_report_ >= kBweConverganceTime) {
        uma_update_state_ = UmaState::DONE;
    }
}
    
} // namespace naivertc
