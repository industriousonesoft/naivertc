#include "rtc/congestion_controller/goog_cc/aimd_rate_control.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr double kDefaultBackoffFactor = 0.85;
constexpr TimeDelta kDefaultRtt = TimeDelta::Millis(200);
    
} // namespace


AimdRateControl::AimdRateControl(Configuration config) 
    : config_(std::move(config)),
      min_configured_bitrate_(DataRate::BitsPerSec(5000)),
      max_configured_bitrate_(DataRate::BitsPerSec(30000)),
      curr_bitrate_(max_configured_bitrate_),
      latest_estimated_throughput_(curr_bitrate_),
      rate_control_state_(RateControlState::HOLD),
      time_last_bitrate_change_(Timestamp::MinusInfinity()),
      time_last_bitrate_decrease_(Timestamp::MinusInfinity()),
      time_first_throughput_estimate_(Timestamp::MinusInfinity()),
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

bool AimdRateControl::ValidEstimate() const {
    return is_bitrate_initialized_;
}

TimeDelta AimdRateControl::GetFeedbackInterval() const {
    // Estimate how often we can send RTCP if we allocate up 
    // to 5% of bandwidth to feedback.
    const int64_t kRtcpSizeInBytes = 80;
    const DataRate rtcp_bitrate = curr_bitrate_ * 0.05;
    const TimeDelta interval = TimeDelta::Millis(kRtcpSizeInBytes * 8 / (rtcp_bitrate.bps() * 1000));
    const TimeDelta kMinFeedbackInterval = TimeDelta::Millis(200);
    const TimeDelta kMaxFeedbackInterval = TimeDelta::Millis(1000);
    return interval.Clamped(kMinFeedbackInterval, kMaxFeedbackInterval);
}

bool AimdRateControl::TimeToReduceFurther(Timestamp at_time, DataRate estimated_throughput) const {
    const TimeDelta bitrate_reduction_interval = rtt_.Clamped(TimeDelta::Millis(10), TimeDelta::Millis(200));
    if (at_time - time_last_bitrate_change_ >= bitrate_reduction_interval) {
        return true;
    }
    if (is_bitrate_initialized_) {
        const DataRate threshold = DataRate::BitsPerSec(0.5 * LatestEstimate().bps());
        return estimated_throughput < threshold;
    }
    return false;
}

bool AimdRateControl::InitialTimeToReduceFurther(Timestamp at_time) const {
    if (!config_.initial_backoff_interval) {
        return ValidEstimate() && TimeToReduceFurther(at_time, LatestEstimate() / 2 - DataRate::BitsPerSec(1));
    }
    if (time_last_bitrate_decrease_.IsInfinite() ||
        at_time - time_last_bitrate_decrease_ >= *config_.initial_backoff_interval) {
        return true;
    }
    return false;
}

DataRate AimdRateControl::Update(BandwidthUsage bw_state, 
                                 std::optional<DataRate> estimated_throughput, 
                                 Timestamp at_time) {
    if (!is_bitrate_initialized_) {
        const TimeDelta kInitializationTime = TimeDelta::Seconds(5);
        if (time_first_throughput_estimate_.IsInfinite()) {
            if (estimated_throughput) {
                time_first_throughput_estimate_ = at_time;
            }
        } else if (at_time - time_first_throughput_estimate_ > kInitializationTime &&
                   estimated_throughput) {
            curr_bitrate_ = estimated_throughput.value();
            is_bitrate_initialized_ = true;
        }
    }

    // TODO: ChangeBitrate

}

DataRate AimdRateControl::GetNearMaxIncreaseRateBps() const {
    assert(!curr_bitrate_.IsZero());
    // Assume the FPS is 30.
    double bits_per_frame = curr_bitrate_.bytes_per_sec() / 30.0;
    double packets_per_frame = std::ceil(bits_per_frame / 9600.0 /* 8.0 * 1200.0 = bits_per_bytes * packet_size */);
    double avg_packet_size_bits = bits_per_frame / packets_per_frame;

    // Approximate the over-use estimator delay to 100 ms.
    TimeDelta response_time = rtt_ + TimeDelta::Millis(100);
    // Additive increases of bitrate: Add one packet per response time when no over-use is detected.
    DataRate increase_rate_bps = DataRate::BitsPerSec(avg_packet_size_bits / response_time.seconds());
    const DataRate kMinIncreaseRateBps = DataRate::BitsPerSec(4000);
    return std::max(kMinIncreaseRateBps, increase_rate_bps);
}

TimeDelta AimdRateControl::GetExpectedBandwidthPeriod() const {
    const TimeDelta kMinPeriod = TimeDelta::Seconds(2);
    const TimeDelta kDefaultPeriod = TimeDelta::Seconds(3);
    const TimeDelta kMaxPeriod = TimeDelta::Seconds(50);

    DataRate increase_rate = GetNearMaxIncreaseRateBps();
    if (!last_decrease_) {
        return kDefaultPeriod;
    }
    double time_to_recover_decrease_seconds = last_decrease_->bps() / increase_rate.bps();
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
        auto time_since_last_update = at_time - last_time;
        alpha = pow(alpha, std::min<double>(time_since_last_update.seconds<double>(), 1.0));
    }
    DataRate multiplicative_increase = std::max(curr_bitrate * (alpha - 1.0), DataRate::BitsPerSec(1000));
    return multiplicative_increase;
}

DataRate AimdRateControl::AdditiveRateIncrease(Timestamp at_time, 
                                               Timestamp last_time) const {
    double time_period_seconds = (at_time - last_time).seconds<double>();
    double data_rate_increase_bps = GetNearMaxIncreaseRateBps().bps() * time_period_seconds;
    return DataRate::BitsPerSec(data_rate_increase_bps);
}

// +----+--------+-----------+------------+--------+
// |     \ State |   Hold    |  Increase  |Decrease|
// |      \      |           |            |        |
// | Signal\     |           |            |        |
// +--------+----+-----------+------------+--------+
// |  Over-use   | Decrease  |  Decrease  |        |
// +-------------+-----------+------------+--------+
// |  Normal     | Increase  |            |  Hold  |
// +-------------+-----------+------------+--------+
// |  Under-use  |           |   Hold     |  Hold  |
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
        if (rate_control_state_ != RateControlState::DECREASE) {
            rate_control_state_ = RateControlState::DECREASE;
        }
        break;
    case BandwidthUsage::UNDERUSING:
        rate_control_state_ = RateControlState::HOLD;
        break;
    default:
        RTC_NOTREACHED();
    }
}
    
} // namespace naivert 
