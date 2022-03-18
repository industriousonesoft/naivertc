#include "rtc/congestion_control/send_side/goog_cc/loss_based/loss_report_based_bwe.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr int kMinLossReportWindow = 20;
// Expecting that RTCP feedback is sent uniformly within [0.5, 1.5]s intervals.
constexpr TimeDelta kMaxRtcpFeedbackInterval = TimeDelta::Millis(5000);
constexpr TimeDelta kBweDecreaseInterval = TimeDelta::Millis(300);
    
} // namespace

LossReportBasedBwe::LossReportBasedBwe(Configuration config) 
    : config_(config),
      fraction_loss_(0),
      accumulated_lost_packets_(0),
      accumulated_packets_(0),
      has_decreased_since_last_fraction_loss_(false),
      time_last_fraction_loss_update_(Timestamp::MinusInfinity()),
      time_last_decrease_(Timestamp::MinusInfinity()) {}
    
LossReportBasedBwe::~LossReportBasedBwe() {}

uint8_t LossReportBasedBwe::fraction_loss() const {
    return fraction_loss_;
}

void LossReportBasedBwe::OnPacketsLostReport(int64_t num_packets_lost,
                                             int64_t num_packets,
                                             Timestamp report_time) {
    assert(num_packets >= num_packets_lost);
    accumulated_packets_ += num_packets;
    accumulated_lost_packets_ += num_packets_lost;

    PLOG_VERBOSE_IF(true) << "num_packets_lost=" << num_packets_lost
                          << " - num_packets=" << num_packets;

    // Don't generate a loss rate until it can be based on enough packets.
    if (accumulated_packets_ < kMinLossReportWindow) {
        return;
    }
    int64_t lost_q8 = accumulated_lost_packets_ << 8;
    fraction_loss_ = std::min<uint8_t>(lost_q8 / accumulated_packets_, 255);

    // Reset accumulator.
    accumulated_lost_packets_ = 0;
    accumulated_packets_ = 0;
    time_last_fraction_loss_update_ = report_time;
    has_decreased_since_last_fraction_loss_ = false;
}

 std::pair<DataRate, RateControlState> LossReportBasedBwe::Estimate(DataRate min_bitrate,
                                                                    DataRate expected_birate,
                                                                    TimeDelta rtt,
                                                                    Timestamp at_time) {
    RateControlState state = RateControlState::HOLD;

    // No loss reports have been received yet.
    if (time_last_fraction_loss_update_.IsInfinite()) {
        return {expected_birate, state};
    }
                        
    // Check if the last reports is still available.
    if (!IsLossReportExpired(at_time)) {
        // We only care about loss above a given bitrate threshold.
        float loss_ratio = fraction_loss_ / 256.0f;
        // Increase
        // We only make decisions based on loss when the bitrate is above a
        // threshold. This is a crude way of handling loss which is uncorrelated
        // to congestion.
        if (expected_birate < config_.max_bitrate_threshold || loss_ratio <= config_.low_loss_threshold) {
            // Loss < 2%: Increase rate by 8% of the min bitrate in the last
            // kBweIncreaseInterval.
            // Note that by remembering the bitrate over the last second one can
            // rampup up one second faster than if only allowed to start ramping
            // at 8% per second rate now. E.g.:
            //   If sending a constant 100kbps it can rampup immediately to 108kbps
            //   whenever a receiver report is received with lower packet loss.
            //   If instead one would do: expected_birate *= 1.08^(delta time),
            //   it would take over one second since the lower packet loss to achieve
            //   108kbps.
            DataRate new_bitrate = DataRate::BitsPerSec(1.08 * min_bitrate.bps() + 0.5);

            // Add 1 kbps extra, just to make sure that we do not get stuck
            // (gives a little extra increase at low rates, negligible at higher
            // rates).
            new_bitrate += DataRate::KilobitsPerSec(1);

            return {new_bitrate, RateControlState::INCREASE};
        } 
        // Hold or decrease
        else if (expected_birate > config_.max_bitrate_threshold) {
            // Hold
            if (loss_ratio <= config_.high_loss_threshold) {
                // Loss ratio between 2% ~ 10%, do nothing.
            }
            // Decrease
            else {
                // Loss ratio > 10%: Limit the rate decreases to once a kBweDecreaseInterval + RTT.
                if (!has_decreased_since_last_fraction_loss_ &&
                    (at_time - time_last_decrease_) > (kBweDecreaseInterval + rtt)) {
                    time_last_decrease_ = at_time;

                    // new_bitrate = expected_birate * (1 - 0.5 * loss_ratio)
                    DataRate new_bitrate = expected_birate * static_cast<double>((512 - fraction_loss_) / 512.0);
                    has_decreased_since_last_fraction_loss_ = true;

                    return {new_bitrate, RateControlState::DECREASE};
                }
            }
        }
    }
    return {expected_birate, RateControlState::DECREASE};
}

// Private methods
bool LossReportBasedBwe::IsLossReportExpired(Timestamp at_time) const {
    TimeDelta elapsed_time = at_time - time_last_fraction_loss_update_;
    return elapsed_time >= 1.2 * kMaxRtcpFeedbackInterval;
}
    
} // namespace naivertc
