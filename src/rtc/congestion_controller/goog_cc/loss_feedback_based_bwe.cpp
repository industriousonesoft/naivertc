#include "rtc/congestion_controller/goog_cc/loss_feedback_based_bwe.hpp"

namespace naivertc {
namespace {

// Expecting RTCP feedback to be sent wieht roughly 1s interval.
constexpr TimeDelta kDefaultRtcpFeedbackInterval = TimeDelta::Millis(1000);
// A 5s gap between two RTCP feedbacks indicates a channel outage.
constexpr int kMaxRtcpFeedbackIntervalMs = 5000;
// The valid period of a RTCP feeback.
constexpr TimeDelta kRtcpFeedbackValidPeriod = TimeDelta::Millis<int64_t>(1.2 * kMaxRtcpFeedbackIntervalMs);

double CalcIncreaseFactor(const LossFeedbackBasedBwe::Configuration& config, TimeDelta rtt) {
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
    // loss_ratio = (loss_bandwidth_balance / bitrate)^exponent
    return pow(loss_bandwidth_balance / bitrate, exponent);
}

DataRate BitrateFromLossRatio(double loss_ratio, 
                              DataRate loss_bandwidth_balance, 
                              double exponent) {
    if (exponent <= 0) {
        return DataRate::Infinity();
    }
    if (loss_ratio < 1e-5) {
        return DataRate::Infinity();
    }
    // bitrate = loss_bandwidth_balance * (1 / (loss_ratio^1/exponent))
    return loss_bandwidth_balance * pow(loss_ratio, -1.0 / exponent);
}

double ExponentialSmoothingFactor(TimeDelta window_size, TimeDelta interval) {
    if (window_size <= TimeDelta::Zero()) {
        return 1.0;
    }
    // x = interval / window
    // factor = 1 - e^-x= 1 - 1/e^x
    // NOTE: The growth of factor has a positive relation with interval length.
    return 1.0 - exp(interval / window_size * -1.0);
}
    
} // namespace

LossFeedbackBasedBwe::LossFeedbackBasedBwe(Configuration config) 
    : config_(std::move(config)),
      avg_loss_ratio_(0.f),
      avg_loss_ratio_max_(0.f),
      last_loss_ratio_(0.f),
      has_decreased_since_last_loss_report_(false),
      loss_based_bitrate_(DataRate::Zero()),
      ack_bitrate_max_(DataRate::Zero()),
      time_ack_bitrate_last_update_(Timestamp::MinusInfinity()),
      time_last_decrease_(Timestamp::MinusInfinity()),
      time_last_loss_packet_report_(Timestamp::MinusInfinity()) {}

LossFeedbackBasedBwe::~LossFeedbackBasedBwe() = default;

bool LossFeedbackBasedBwe::InUse() const {
    return time_last_loss_packet_report_.IsFinite();
}

void LossFeedbackBasedBwe::SetInitialBitrate(DataRate bitrate) {
    loss_based_bitrate_ = bitrate;
    avg_loss_ratio_ = 0.f;
    avg_loss_ratio_max_ = 0.f;
}

void LossFeedbackBasedBwe::OnPacketFeedbacks(const std::vector<PacketResult>& packet_feedbacks, 
                                             Timestamp at_time) {
    if (packet_feedbacks.empty()) {
        return;
    }
    int loss_count = 0;
    for (const auto& pkt_feedback : packet_feedbacks) {
        loss_count += pkt_feedback.IsLost() ? 1 : 0;
    }
    double loss_ratio = static_cast<double>(loss_count) / packet_feedbacks.size();
    const TimeDelta elapsed_time = time_last_loss_packet_report_.IsFinite()
                                  ? at_time - time_last_loss_packet_report_
                                  : kDefaultRtcpFeedbackInterval;
    time_last_loss_packet_report_ = at_time;
    has_decreased_since_last_loss_report_ = false;
    // NOTE: 由于packet_feedbacks在处理时设置了时间窗口，导致太旧的包反馈会被丢弃，所以反馈的只是窗口内的丢包情况。
    // 因此，此处计算丢包率采样了指数滑动平均数的方式，在历史值的基础上给予新数据的更大权重，这样可得到更接近于真实值的丢包率。
    // Exponetial smoothing
    avg_loss_ratio_ += ExponentialSmoothingFactor(config_.loss_window, elapsed_time) * (loss_ratio - avg_loss_ratio_);
    // The max loss ratio is self-adaptive, which adapts to the average.
    if (avg_loss_ratio_ > avg_loss_ratio_max_) {
        avg_loss_ratio_max_ = avg_loss_ratio_;
    } else {
        double smoothing_factor = ExponentialSmoothingFactor(config_.loss_max_window, elapsed_time);
        avg_loss_ratio_max_ -= smoothing_factor * (avg_loss_ratio_max_ - avg_loss_ratio_);
    }
    last_loss_ratio_ = loss_ratio;
}

void LossFeedbackBasedBwe::OnAcknowledgedBitrate(DataRate ack_bitrate, 
                                                 Timestamp at_time) {
    
    if (ack_bitrate > ack_bitrate_max_) {
        ack_bitrate_max_ = ack_bitrate;
    } else {
        // The time has elapsed since last time.
        TimeDelta elapsed_time = time_ack_bitrate_last_update_.IsFinite() ? at_time - time_ack_bitrate_last_update_
                                                                          : kDefaultRtcpFeedbackInterval;
        double smoothing_factor = ExponentialSmoothingFactor(config_.ack_rate_max_window, elapsed_time);
        ack_bitrate_max_ -= smoothing_factor * (ack_bitrate_max_ - ack_bitrate);
    }
    time_ack_bitrate_last_update_ = at_time;
}

 std::pair<DataRate, RateControlState> LossFeedbackBasedBwe::Estimate(DataRate min_bitrate,
                                                                      DataRate expected_birate,
                                                                      TimeDelta rtt,
                                                                      Timestamp at_time) {
    if (loss_based_bitrate_.IsZero()) {
        // The initial bitrate is not set yet.
        loss_based_bitrate_ = expected_birate;
    }
    
    RateControlState state = RateControlState::HOLD;

    // Only increase if loss ratio has beed low for some time.
    const double loss_ratio_estimate_for_increase = avg_loss_ratio_max_;
    // Avoid multiple decreases from averaging over one loss spike.
    const double loss_ratio_estimate_for_decrease = std::min(avg_loss_ratio_, last_loss_ratio_);
    // 允许降低码率操作的需要同时满足一下两个条件：
    // 1、次数限制：在下一次反馈到来之前只允许操作一次
    // 2、时长限制：距离上一次操作的时长>=decrease_interval
    const bool allow_to_decrease = !has_decreased_since_last_loss_report_ && (at_time - time_last_decrease_ >= rtt + config_.decrease_interval);
    // If packet lost reports are too old, don't increase bitrate.
    const bool loss_report_valid = at_time - time_last_loss_packet_report_ < kRtcpFeedbackValidPeriod;

    // Reset
    if (loss_report_valid && config_.allow_resets &&
        loss_ratio_estimate_for_increase < ThresholdToReset()) {
        loss_based_bitrate_ = expected_birate;
    } 
    // Increase
    else if (loss_report_valid && loss_ratio_estimate_for_increase < ThresholdToIncrease()) {
        // Increase bitrate by RTT-adptive ratio.
        DataRate new_bibtrate = min_bitrate * CalcIncreaseFactor(config_, rtt) + config_.increase_offset;

        const DataRate increased_bibtrate_cap = BitrateFromLossRatio(loss_ratio_estimate_for_increase,
                                                                     config_.loss_bandwidth_balance_increase,
                                                                     config_.loss_bandwidth_balance_exponent);
        // Limit the new bitrate below the cap.
        new_bibtrate = std::min(new_bibtrate, increased_bibtrate_cap);
        if (new_bibtrate > loss_based_bitrate_) {
            loss_based_bitrate_ = new_bibtrate;
        }
        state = RateControlState::INCREASE;
    } 
    // Decrease
    else if (loss_ratio_estimate_for_decrease > ThresholdToDecrease() && allow_to_decrease) {
        // Decrease bitrate by the fixed ratio.
        DataRate new_bitrate = config_.decrease_factor * ack_bitrate_max_;
        const DataRate decreased_bitrate_floor = BitrateFromLossRatio(loss_ratio_estimate_for_decrease,
                                                                      config_.loss_bandwidth_balance_decrease,
                                                                      config_.loss_bandwidth_balance_exponent);
        
        // Limit the new bitrate above the floor.
        new_bitrate = std::max(new_bitrate, decreased_bitrate_floor);
        if (new_bitrate < loss_based_bitrate_) {
            time_last_decrease_ = at_time;
            has_decreased_since_last_loss_report_ = true;
            loss_based_bitrate_ = new_bitrate;
        }
        state = RateControlState::DECREASE;
    } else {
        // Hold
    }
    return {loss_based_bitrate_, state};
}

// Private methods
double LossFeedbackBasedBwe::ThresholdToReset() const {
    return LossRatioFromBitrate(loss_based_bitrate_,
                                config_.loss_bandwidth_balance_reset,
                                config_.loss_bandwidth_balance_exponent);
}

double LossFeedbackBasedBwe::ThresholdToIncrease() const {
    return LossRatioFromBitrate(loss_based_bitrate_,
                                config_.loss_bandwidth_balance_increase,
                                config_.loss_bandwidth_balance_exponent);
}

double LossFeedbackBasedBwe::ThresholdToDecrease() const {
    return LossRatioFromBitrate(loss_based_bitrate_,
                                config_.loss_bandwidth_balance_decrease,
                                config_.loss_bandwidth_balance_exponent);
}
    
} // namespace naivertc
