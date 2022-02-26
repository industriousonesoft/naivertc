#include "rtc/congestion_controller/goog_cc/aimd_rate_control.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

// the backoff factor is typically chosen to be in the interval [0.8, 0.95], 
// 0.85 is the RECOMMENDED value.
constexpr double kDefaultBackoffFactor = 0.85;
constexpr TimeDelta kDefaultRtt = TimeDelta::Millis(200);
constexpr DataRate kMinIncreaseRatePerSecond = DataRate::BitsPerSec(4000); // 4 kbps
    
} // namespace

AimdRateControl::AimdRateControl(Configuration config, bool send_side) 
    : config_(std::move(config)),
      send_side_(send_side),
      min_configured_bitrate_(config_.min_bitrate),
      curr_bitrate_(config_.max_bitrate),
      latest_estimated_throughput_(curr_bitrate_),
      rate_control_state_(RateControlState::HOLD),
      time_last_bitrate_change_(Timestamp::MinusInfinity()),
      time_last_bitrate_decrease_(Timestamp::MinusInfinity()),
      time_first_throughput_arrive_(Timestamp::MinusInfinity()),
      is_bitrate_initialized_(false),
      backoff_factor_(kDefaultBackoffFactor),
      in_alr_(false),
      rtt_(kDefaultRtt) {

    PLOG_INFO << "Using AIMD rate control with back off factor: " << backoff_factor_;
}

AimdRateControl::~AimdRateControl() {}

void AimdRateControl::set_rtt(TimeDelta rtt) {
    rtt_ = rtt;
}

void AimdRateControl::set_in_alr(bool in_alr) {
    in_alr_ = in_alr;
}

DataRate AimdRateControl::LatestEstimate() const {
    return curr_bitrate_;
}

void AimdRateControl::SetStartBitrate(DataRate start_bitrate) {
    curr_bitrate_ = start_bitrate;
    latest_estimated_throughput_ = curr_bitrate_;
    is_bitrate_initialized_ = true;
}

void AimdRateControl::SetMinBitrate(DataRate min_bitrate) {
    min_configured_bitrate_ = min_bitrate;
    curr_bitrate_ = std::max(min_bitrate, curr_bitrate_);
}

void AimdRateControl::SetEstimate(DataRate bitrate, Timestamp at_time) {
    is_bitrate_initialized_ = true;
    DataRate prev_bitrate = curr_bitrate_;
    curr_bitrate_ = bitrate;
    time_last_bitrate_change_ = at_time;
    if (curr_bitrate_ < prev_bitrate) {
        time_last_bitrate_decrease_ = at_time;
    }
}

bool AimdRateControl::ValidEstimate() const {
    return is_bitrate_initialized_;
}

TimeDelta AimdRateControl::GetFeedbackInterval() const {
    // Used in receive-side estimation.
    assert(send_side_ == false);
    // Estimate how often we can send RTCP if we allocate up 
    // to 5% of bandwidth to feedback. 

    // FIXME: Why do we assume the size of RTCP packet is 80? 
    // Do we assume the RTCP packt is a Reduced-Size RTCP, as it can 
    // become at least 70-80 bytes smaller than the compound packet?
    // see https://datatracker.ietf.org/doc/html/rfc5506#section-3.3
    const int64_t kRtcpSizeInBits = 640; // 80 * 8
    const TimeDelta kMinFeedbackInterval = TimeDelta::Millis(200);
    const TimeDelta kMaxFeedbackInterval = TimeDelta::Millis(1000);
    const DataRate rtcp_bitrate = curr_bitrate_ * 0.05;
    const TimeDelta interval = TimeDelta::Millis((kRtcpSizeInBits * 1000) / rtcp_bitrate.bps());
    return interval.Clamped(kMinFeedbackInterval, kMaxFeedbackInterval);
}

bool AimdRateControl::CanReduceFurther(Timestamp at_time, DataRate estimated_throughput) const {
    // Check if it's time to reduce further.
    if (TimeToReduceFurther(at_time)) {
        return true;
    }
    // Check if the estimate bitrate is much larger than throughput.
    if (is_bitrate_initialized_) {
        return SufficientToReduceFurther(estimated_throughput);
    }
    return false;
}

bool AimdRateControl::CanReduceFurtherInStartPhase(Timestamp at_time) const {
    if (!config_.initial_backoff_interval) {
        // If the `initial_backoff_interval` is not set, 
        // we can reduce further when the bitrate is initialized.
        return is_bitrate_initialized_;
    }
    // TODO: We could use the RTT (clamped to suitable limits) 
    // instead of a fixed bitrate_reduction_interval.
    if (time_last_bitrate_decrease_.IsInfinite() ||
        at_time - time_last_bitrate_decrease_ >= *config_.initial_backoff_interval) {
        return true;
    }
    return false;
}

DataRate AimdRateControl::Update(BandwidthUsage bw_state, 
                                 std::optional<DataRate> estimated_throughput, 
                                 Timestamp at_time) {
    
    // Set the initial bitrate to what we're receiving in intiial phrase (5s).
    if (!is_bitrate_initialized_ && estimated_throughput) {
        const TimeDelta kInitializationTime = TimeDelta::Seconds(5); // 5s
        if (time_first_throughput_arrive_.IsInfinite()) {
            // The time of the first throughput arrived.
            time_first_throughput_arrive_ = at_time;
        } else if (at_time - time_first_throughput_arrive_ > kInitializationTime) {
            curr_bitrate_ = estimated_throughput.value();
            is_bitrate_initialized_ = true;
        }
    }

    ChangeBitrate(bw_state, estimated_throughput, at_time);
    return curr_bitrate_;
}

DataRate AimdRateControl::GetNearMaxIncreaseRatePerSecond() const {
    assert(!curr_bitrate_.IsZero());
    // Assumed the FPS is 30.
    double bits_per_frame = curr_bitrate_.bps() / 30.0;
    double packets_per_frame = std::ceil(bits_per_frame / 9600.0 /* bits_per_packet = bits_per_bytes * packet_size_bytes = 8.0 * 1200.0 */);
    double avg_packet_size_in_bits = bits_per_frame / packets_per_frame;

    // The response_time interval is estimated as the round-trip time
    // plus 100 ms as an estimate of over-use estimator and detector
    // reaction time.
    // see https://datatracker.ietf.org/doc/html/draft-ietf-rmcat-gcc-02#page-10
    TimeDelta response_time = rtt_ + TimeDelta::Millis(100);
    // FIXME: why do we need to increase more slowly when using a adative threshold?
    if (config_.adaptive_threshold_enabled) {
        response_time = response_time * 2;
    }
    // Increase slightly (one packet every response time) when used bandwidth 
    // is near the link capacity (assuming the max bitrate).
    DataRate increase_rate_per_second = DataRate::BitsPerSec(avg_packet_size_in_bits * 1000.0 / response_time.ms());
    return std::max(kMinIncreaseRatePerSecond, increase_rate_per_second);
}

TimeDelta AimdRateControl::GetExpectedBandwidthPeriod() const {
    const TimeDelta kDefaultPeriod = TimeDelta::Seconds(3);
    const TimeDelta kMinPeriod = TimeDelta::Seconds(2);
    const TimeDelta kMaxPeriod = TimeDelta::Seconds(50);

    if (!last_decreased_bitrate_) {
        return kDefaultPeriod;
    }
    DataRate increase_rate_per_second = GetNearMaxIncreaseRatePerSecond();
    // Calculate the time in second to recover from `DECREASE` state.
    double time_to_recover_decrease_seconds = last_decreased_bitrate_->bps<double>() / increase_rate_per_second.bps<double>();
    TimeDelta period = TimeDelta::Seconds(time_to_recover_decrease_seconds);
    return period.Clamped(kMinPeriod, kMaxPeriod);
}

// Private methods
DataRate AimdRateControl::ClampBitrate(DataRate new_bitrate) const {
    return std::max(new_bitrate, min_configured_bitrate_);
}

DataRate AimdRateControl::MultiplicativeRateIncrease(Timestamp at_time, 
                                                     Timestamp last_time, 
                                                     DataRate curr_bitrate) const {
    double alpha = 1.08;
    if (last_time.IsFinite()) {
        double time_since_last_update = (at_time - last_time).seconds<double>();
        // alpha = 1.08^min(time_since_last_update_s, 1.0)
        alpha = pow(alpha, std::min<double>(time_since_last_update, 1.0));
    }
    // During multiplicative increase, the estimate is increased by at most 8% per second.
    // NOTE: The min bitrate increased multiplicatively is limited to 1000 bps.
    DataRate multiplicative_increase = std::max(curr_bitrate * (alpha - 1.0), DataRate::BitsPerSec(1000));
    return multiplicative_increase;
}

DataRate AimdRateControl::AdditiveRateIncrease(Timestamp at_time, 
                                               Timestamp last_time) const {
    double time_since_last_update = (at_time - last_time).seconds<double>();
    // `GetNearMaxIncreaseRatePerSecond` is used to get a slightly slower 
    // slope for the additive increase at lower bitrate.
    double data_rate_increase_bps = GetNearMaxIncreaseRatePerSecond().bps() * time_since_last_update;
    return DataRate::BitsPerSec(data_rate_increase_bps);
}

void AimdRateControl::ChangeBitrate(BandwidthUsage bw_state, 
                                    std::optional<DataRate> new_estimated_throughput, 
                                    Timestamp at_time) {
    std::optional<DataRate> new_bitrate;
    // Update the throughput if we can.
    DataRate estimated_throughput = new_estimated_throughput.value_or(latest_estimated_throughput_);
    if (new_estimated_throughput) {
        latest_estimated_throughput_ = new_estimated_throughput.value();
    }

    // An over-use should always trigger us to reduce the bitrate,
    // even though we have not yet established our first estimate. 
    // By acting on the over-use, we will end up with a valid estimate.
    if (!is_bitrate_initialized_ && bw_state != BandwidthUsage::OVERUSING) {
        return;
    }

    ChangeState(bw_state, at_time);

    switch (rate_control_state_) {
    case RateControlState::HOLD:
        break;
    case RateControlState::INCREASE: {
        // If throughput increases above three standard deviations of the average
        // max bitrate, we assume that the current congestion level has changed,
        // at which point we reset the average max bitrate and use multiplicative
        // increase.
        if (estimated_throughput > link_capacity_.UpperBound()) {
            link_capacity_.Reset();
        }

        // We limit the new bitrate based on the throughput to avoid unlimited bitrate
        // increases. 
        // We allow a bit more lag at very low rates to not too easily get stuck if 
        // the encoder produces uneven outputs.
        const DataRate throughput_based_limit = 1.5 * estimated_throughput + DataRate::KilobitsPerSec(10);

        // Do not increase the delay-based estimate in alr since the estimator
        // will not be able to get transport feedback necessary to detect if
        // the new estimate is correct.
        // If we have previously increased above the limit (for instance due to
        // probing), we don't allow further changes.
        if (curr_bitrate_ < throughput_based_limit && !DontIncreaseInAlr()) {
            DataRate increased_bitrate = DataRate::MinusInfinity();
            if (link_capacity_.Estimate()) {
                // The |link_capacity| estimate is reset if the measured throughput
                // is too far from the estimate. We can therefore assume that our
                // target rate is reasonably close to link capacity and use additive
                // increase.
                DataRate additive_increase = AdditiveRateIncrease(at_time, time_last_bitrate_change_);
                increased_bitrate = curr_bitrate_ + additive_increase;
            } else {
                // If we don't have an estimate of the link capacity, switch to 'slow-start mode',
                // and use faster ramp up to discover the capacity.
                DataRate multiplicative_increase = MultiplicativeRateIncrease(at_time, time_last_bitrate_change_, curr_bitrate_);
                increased_bitrate = curr_bitrate_ + multiplicative_increase;
            }
            new_bitrate = std::min(increased_bitrate, throughput_based_limit);
        }
        
        time_last_bitrate_change_ = at_time;
        break;
    }
    case RateControlState::DECREASE: {
        DataRate decreased_bitrate = DataRate::PlusInfinity();
        
        // Set bitrate to something slightly lower than the measured throughput
        // to get rid of any self-induced delay.
        decreased_bitrate = estimated_throughput * backoff_factor_;
        // Fix the bitrate further if it's still higher than the current bitrate,
        // since |decreased_bitrate| is supposed to lower than the |curr_bitrate_|.
        if (decreased_bitrate > curr_bitrate_ && !config_.link_capacity_fix) {
            // TODO: The link_capacity estimate may be based on old throughput measurement.
            // so Relying on them may lead to unnecessary BWE drop. 
            // NOTE: link_capacity中采用的估算算法是滑动平均滤波，且旧数据（吞吐量）的权重（0.95）远高于新数据，
            // 导致对于当下的网络负载反应不灵敏。因此不适合用于当前码率的计算。
            if (auto estimate = link_capacity_.Estimate()) {
                decreased_bitrate = DataRate::BitsPerSec(backoff_factor_ * estimate->bps());
            }
        }
        
        // 理论上，检测到带宽状态为overuse时，当前码率略高于吞吐量，那么基于吞吐量来更新当前码率是合理的。
        // 极端情况下，当前码率远小于吞吐量时认被检测出overuse状态，那么则继续保持当前码率（或最小码率）。
        // Avoid increasing the rate when over-using.
        if (decreased_bitrate < curr_bitrate_) {
            new_bitrate = decreased_bitrate;
        } else {
            // Remain in the current bitrate.
        }

        // Calculate the last decreased bitrate.
        if (is_bitrate_initialized_ && estimated_throughput < curr_bitrate_) {
            if (!new_bitrate) {
                last_decreased_bitrate_ = DataRate::Zero();
            } else {
                last_decreased_bitrate_ = curr_bitrate_ - new_bitrate.value();
            }
        }

        if (estimated_throughput < link_capacity_.LowerBound()) {
            // The current throughput is far from the estimated link capacity.
            // Clear the estimate to allow an immediate update in OnOveruseDetected.
            link_capacity_.Reset();
        }

        // 网络过载的情况下的吞吐量估计值更接近于真实的网络负载值。
        link_capacity_.OnOveruseDetected(estimated_throughput);

        is_bitrate_initialized_ = true;
        // Stay on hold until the pipes are cleared.
        rate_control_state_ = RateControlState::HOLD;
        time_last_bitrate_change_ = at_time;
        time_last_bitrate_decrease_ = at_time;
        break;
    }
    default:
        RTC_NOTREACHED();
    }
    curr_bitrate_ = ClampBitrate(new_bitrate.value_or(curr_bitrate_));
}

// The state transitions (with blank fields meaning "remain in state")
// are:
// +----+--------+-----------+------------+--------+
// |     \ State |   Hold    |  Increase  |Decrease|
// |      \      |           |            |        |
// | Signal\     |           |            |        |
// +--------+----+-----------+------------+--------+
// |  Over-use   | Decrease  |  Decrease  |    -   |
// +-------------+-----------+------------+--------+
// |  Normal     | Increase  |     -      |  Hold  |
// +-------------+-----------+------------+--------+
// |  Under-use  |    -      |   Hold     |  Hold  |
// +-------------+-----------+------------+--------+
void AimdRateControl::ChangeState(BandwidthUsage bw_state, 
                                  Timestamp at_time) {
    switch (bw_state) {
    case BandwidthUsage::NORMAL:
        if (rate_control_state_ == RateControlState::HOLD) {
            time_last_bitrate_change_ = at_time;
            rate_control_state_ = RateControlState::INCREASE;
        }
        break;
    case BandwidthUsage::OVERUSING:
        rate_control_state_ = RateControlState::DECREASE;
        break;
    case BandwidthUsage::UNDERUSING:
        rate_control_state_ = RateControlState::HOLD;
        break;
    default:
        RTC_NOTREACHED();
    }
}

bool AimdRateControl::DontIncreaseInAlr() const {
    return send_side_ && in_alr_ && config_.no_bitrate_increase_in_alr;
}

bool AimdRateControl::TimeToReduceFurther(Timestamp at_time) const {
    const TimeDelta clamped_rtt = rtt_.Clamped(TimeDelta::Millis(10), TimeDelta::Millis(200));
    // If the bitrate estimate hasn't been changed for more than an RTT.
    return at_time - time_last_bitrate_change_ >= clamped_rtt;
}

bool AimdRateControl::SufficientToReduceFurther(DataRate estimated_throughput) const {
    if (is_bitrate_initialized_) {
        // If the estimated_throughput is less than half of the current
        // estimate.
        // TODO: Investigate consequences of increasing 
        // the threshold to 0.95 * `curr_bitrate_`.
        const auto threshold = 0.5 * curr_bitrate_;
        return estimated_throughput < threshold;
    }
    return false;
}
    
} // namespace naivert 
