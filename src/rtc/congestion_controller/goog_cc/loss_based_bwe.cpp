#include "rtc/congestion_controller/goog_cc/loss_based_bwe.hpp"

namespace naivertc {
namespace {

// Expecting RTCP feedback to be sent wieht roughly 1s interval.
constexpr TimeDelta kDefaultRtcpFeedbackInterval = TimeDelta::Millis(1000);
// A 5s gap between two RTCP feedbacks indicates a channel outage.
constexpr int kMaxRtcpFeedbackIntervalMs = 5000;
// The valid period of a RTCP feeback.
constexpr TimeDelta kRtcpFeedbackValidPeriod = TimeDelta::Millis<int64_t>(1.2 * kMaxRtcpFeedbackIntervalMs);

double CalcIncreaseFactor(const LossBasedBwe::Configuration& config, TimeDelta rtt) {
    assert(config.increase_low_rtt < config.increase_high_rtt && "On misconfiguration.");
    // Clamp the RTT
    if (rtt < config.increase_low_rtt) {
        rtt = config.increase_low_rtt;
    } else if (rtt > config.increase_high_rtt) {
        rtt = config.increase_high_rtt;
    }
    auto rtt_range = config.increase_high_rtt - config.increase_low_rtt;
    auto rtt_offset = rtt - config.increase_low_rtt;
    // Normalize the RTT offset.
    auto normalized_offset = std::max(0.0, std::min(rtt_offset / rtt_range, 1.0));
    auto factor_range = config.max_increase_factor - config.min_increase_factor;
    // Increase slower when RTT is high.
    return config.min_increase_factor + (1 - normalized_offset) * factor_range;
}

double LossRatioFromBitrate(DataRate bitrate, 
                            DataRate loss_bandwidth_balance, 
                            double exponent) {
    if (loss_bandwidth_balance >= bitrate) {
        return 1.0;
    }
    return pow(loss_bandwidth_balance / bitrate, exponent);
}

DataRate BitrateFromLossRatio(double loss_ratio, 
                              DataRate losss_bandwidth_balance, 
                              double exponent) {
    if (exponent <= 0) {
        return DataRate::Infinity();
    }
    if (loss_ratio < 1e-5) {
        return DataRate::Infinity();
    }
    return losss_bandwidth_balance * pow(loss_ratio, -1.0 / exponent);
}

double ExponentialSmoothingFactor(TimeDelta window_size, TimeDelta interval) {
    if (window_size <= TimeDelta::Zero()) {
        return 1.0;
    }
    // 1 - e^-x(1/e^x)
    // FIXME: Why using the exp?
    return 1.0 - exp(interval / window_size * -1.0);
}
    
} // namespace

LossBasedBwe::LossBasedBwe(Configuration config) 
    : config_(std::move(config)),
      avg_loss_ratio_(0.f),
      avg_loss_ratio_max_(0.f),
      last_loss_ratio_(0.f),
      has_decreased_since_last_loss_report_(false),
      loss_based_bitrate_(DataRate::Zero()),
      ack_bitrate_max_(DataRate::Zero()),
      time_ack_bitrate_last_update_(Timestamp::MinusInfinity()),
      time_last_decrease_(Timestamp::MinusInfinity()),
      time_last_loss_packet_report_arrive_(Timestamp::MinusInfinity()) {}

LossBasedBwe::~LossBasedBwe() = default;

void LossBasedBwe::SetInitialBitrate(DataRate bitrate) {
    loss_based_bitrate_ = bitrate;
    avg_loss_ratio_ = 0.f;
    avg_loss_ratio_max_ = 0.f;
}

void LossBasedBwe::IncomingFeedbacks(const std::vector<PacketResult> packet_feedbacks, 
                                     Timestamp at_time) {
    if (packet_feedbacks.empty()) {
        return;
    }
    int loss_count = 0;
    for (const auto& pkt_feedback : packet_feedbacks) {
        loss_count += pkt_feedback.IsLost() ? 1 : 0;
    }
    last_loss_ratio_ = static_cast<double>(loss_count) / packet_feedbacks.size();
    const TimeDelta elapsed_time = time_last_loss_packet_report_arrive_.IsFinite()
                                  ? at_time - time_last_loss_packet_report_arrive_
                                  : kDefaultRtcpFeedbackInterval;
    time_last_loss_packet_report_arrive_ = at_time;
    has_decreased_since_last_loss_report_ = false;
    // Exponetial smoothing
    avg_loss_ratio_ += ExponentialSmoothingFactor(config_.loss_window, elapsed_time) * (last_loss_ratio_ - avg_loss_ratio_);
    if (avg_loss_ratio_ > avg_loss_ratio_max_) {
        avg_loss_ratio_max_ = avg_loss_ratio_;
    } else {
        double smoothing_factor = ExponentialSmoothingFactor(config_.loss_max_window, elapsed_time);
        avg_loss_ratio_max_ -= smoothing_factor * (avg_loss_ratio_max_ - avg_loss_ratio_);
    }
}

void LossBasedBwe::UpdateAcknowledgedBitrate(DataRate ack_bitrate, 
                                             Timestamp at_time) {
    const TimeDelta elapsed_time = time_ack_bitrate_last_update_.IsFinite()
                                   ? at_time - time_ack_bitrate_last_update_
                                   : kDefaultRtcpFeedbackInterval;
    time_ack_bitrate_last_update_ = at_time;
    if (ack_bitrate > ack_bitrate_max_) {
        ack_bitrate_max_ = ack_bitrate;
    } else {
        double smoothing_factor = ExponentialSmoothingFactor(config_.ack_rate_max_window, elapsed_time);
        ack_bitrate_max_ -= smoothing_factor * (ack_bitrate_max_ - ack_bitrate);
    }
}

std::optional<DataRate> LossBasedBwe::Estimate(DataRate min_bitrate,
                                               DataRate expected_birate,
                                               TimeDelta rtt,
                                               Timestamp at_time) {
    if (time_last_loss_packet_report_arrive_.IsInfinite()) {
        // The first RTCP feedback is not coming yet.
        return std::nullopt;
    }

    if (loss_based_bitrate_.IsZero()) {
        // The initial bitrate is not set yet.
        return expected_birate;
    }

    // Only increase if loss ratio has beed low for some time.
    const double loss_ratio_estimate_for_increase = avg_loss_ratio_max_;
    // Avoid multiple decreases from averaging over one loss spike.
    const double loss_ratio_estimate_for_decrease = std::min(avg_loss_ratio_, last_loss_ratio_);
    // FIXME: How to understand the formula below.
    const bool allow_to_decrease = !has_decreased_since_last_loss_report_ && (at_time - time_last_decrease_ >= rtt + config_.decrease_interval);
    // If packet lost reports are too old, don't increase bitrate.
    const bool loss_report_valid = at_time - time_last_loss_packet_report_arrive_ < kRtcpFeedbackValidPeriod;

    if (loss_report_valid && config_.allow_resets &&
        loss_ratio_estimate_for_increase < ThresholdToReset()) {
        loss_based_bitrate_ = expected_birate;
    } else if (loss_report_valid && loss_ratio_estimate_for_increase < ThresholdToIncrease()) {
        // Increase bitrate by RTT-adptive ratio.
        DataRate new_bibtrate = min_bitrate * CalcIncreaseFactor(config_, rtt) + config_.increase_offset;

        const DataRate increased_bibtrate_cap = BitrateFromLossRatio(loss_ratio_estimate_for_increase,
                                                                         config_.loss_bandwidth_balance_increase,
                                                                         config_.loss_bandwidth_balance_exponent);
        new_bibtrate = std::min(new_bibtrate, increased_bibtrate_cap);
        loss_based_bitrate_ = std::max(new_bibtrate, loss_based_bitrate_);
    } else if (loss_ratio_estimate_for_decrease > ThresholdToDecrease() && allow_to_decrease) {
        // Decrease bitrate by the fixed ratio.
        DataRate new_bitrate =  config_.decrease_factor * ack_bitrate_max_;
        const DataRate decreased_bitrate_floor = BitrateFromLossRatio(loss_ratio_estimate_for_decrease,
                                                                      config_.loss_bandwidth_balance_decrease,
                                                                      config_.loss_bandwidth_balance_exponent);
        
        new_bitrate = std::max(new_bitrate, decreased_bitrate_floor);
        if (new_bitrate < loss_based_bitrate_) {
            time_last_decrease_ = at_time;
            has_decreased_since_last_loss_report_ = true;
            loss_based_bitrate_ = new_bitrate;
        }
    }
    return loss_based_bitrate_;
}

// Private methods
double LossBasedBwe::ThresholdToReset() const {
    return LossRatioFromBitrate(loss_based_bitrate_,
                                config_.loss_bandwidth_balance_reset,
                                config_.loss_bandwidth_balance_exponent);
}

double LossBasedBwe::ThresholdToIncrease() const {
    return LossRatioFromBitrate(loss_based_bitrate_,
                                config_.loss_bandwidth_balance_increase,
                                config_.loss_bandwidth_balance_exponent);
}

double LossBasedBwe::ThresholdToDecrease() const {
    return LossRatioFromBitrate(loss_based_bitrate_,
                                config_.loss_bandwidth_balance_decrease,
                                config_.loss_bandwidth_balance_exponent);
}
    
} // namespace naivertc
