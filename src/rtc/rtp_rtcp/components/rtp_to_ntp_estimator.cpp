#include "rtc/rtp_rtcp/components/rtp_to_ntp_estimator.hpp"
#include "common/array_view.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
// Maximum number of RTCP SR reports to use to map between RTP and NTP.
constexpr size_t kNumRtcpReportsToUse = 20;
// Don't allow NTP timestamps to jump more than 1 hour. Chosen arbitrary as big
// enough to not affect normal use-cases. Yet it is smaller than RTP wrap-around
// half-period (90khz RTP clock wrap-arounds every 13.25 hours). After half of
// wrap-around period it is impossible to unwrap RTP timestamps correctly.
constexpr int kMaxAllowedRtcpNtpIntervalMs = 60 * 60 * 1000;

// No jumps too far into the future in RTP.
constexpr int32_t kMaxAllowedRtcpTimestampIntervalMs = 1 << 25;

} // namespace

RtpToNtpEstimator::Measurement::Measurement(int64_t ntp_time_ms,
                                            int64_t unwrapped_rtp_timestamp) 
    : ntp_time_ms(ntp_time_ms),
      unwrapped_rtp_timestamp(unwrapped_rtp_timestamp) {}

bool RtpToNtpEstimator::Measurement::operator==(const Measurement& other) const {
    // rtp_timestamp = ntp_time_ms * frequency + offset
    // Note: the offset value using a random instead of starting from 0 to prevent attacks.
    // TODO: Why we use || not &&?
    return (ntp_time_ms == other.ntp_time_ms) || 
           (unwrapped_rtp_timestamp == other.unwrapped_rtp_timestamp);
}

// RtpToNtpEstimator
RtpToNtpEstimator::RtpToNtpEstimator() : consecutive_invalid_samples_(0) {}

RtpToNtpEstimator::~RtpToNtpEstimator() {}

bool RtpToNtpEstimator::UpdateMeasurements(uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp_timestamp) {
    NtpTime ntp_time(ntp_secs, ntp_frac);
    if (!ntp_time.Valid()) {
        return false;
    }
    int64_t ntp_time_ms = ntp_time.ToMs();
    int64_t unwrapped_rtp_timestamp = timestamp_unwrapper_.Unwrap(rtp_timestamp);
    Measurement new_measurement(ntp_time_ms, unwrapped_rtp_timestamp);
    if (Contains(new_measurement)) {
        return false;
    }
    bool invalid_sample = false;
    if (!measurements_.empty()) {
        double old_ntp_ms = measurements_.front().ntp_time_ms;
        double old_rtp_timstamp = measurements_.front().unwrapped_rtp_timestamp;
        if (ntp_time_ms <= old_ntp_ms || ntp_time_ms > old_ntp_ms + kMaxAllowedRtcpNtpIntervalMs) {
            invalid_sample = true;
        } else if (unwrapped_rtp_timestamp <= old_rtp_timstamp) {
            PLOG_WARNING << "Newer RTP timestamp is smaller than the older one, dropping";
            invalid_sample = true;
        } else if (unwrapped_rtp_timestamp - old_rtp_timstamp > kMaxAllowedRtcpTimestampIntervalMs) {
            PLOG_WARNING << "Newer RTP timestamp is jump too far into the future, dropping";
            invalid_sample = true;
        }
    }

    if (invalid_sample) {
        ++consecutive_invalid_samples_;
        if (consecutive_invalid_samples_ < kMaxInvalidSamples) {
            return false;
        }
        PLOG_WARNING << "Multiple consecutively invalid RTCP SR reports, clearing measurements.";
        measurements_.clear();
        params_.reset();
    }

    consecutive_invalid_samples_ = 0;

    if (measurements_.size() == kNumRtcpReportsToUse) {
        measurements_.pop_back();
    }

    measurements_.push_front(std::move(new_measurement));

    CalculateParameters();

    return true;
}

std::optional<int64_t> RtpToNtpEstimator::Estimate(int64_t rtp_timestamp) const {
    if (!params_ || params_->frequency_khz == 0.0) {
        return std::nullopt;
    }
    // Unwrap rtp timestamp (uin32_t) to int64 
    int64_t unwrapped_rtp_timestamp = timestamp_unwrapper_.Unwrap(rtp_timestamp);
    // leaner regression: ntp_timestamp_ms = rtp_timestamp / frequency + offset_ms
    double ntp_timestamp_ms = static_cast<double>(unwrapped_rtp_timestamp) / params_->frequency_khz + params_->offset_ms + 0.5f;

    if (ntp_timestamp_ms >= 0) {
        return ntp_timestamp_ms;
    }

    return std::nullopt;;
}

// Private methods
void RtpToNtpEstimator::CalculateParameters() {
    double slope, offset;
    if (!LinearRegression(&slope, &offset)) {
        return;
    }
    params_.emplace(1 / slope, offset);
}

bool RtpToNtpEstimator::Contains(const Measurement& measurement) const {
    for (const auto& item : measurements_) {
        if (measurement == item) {
            return true;
        }
    }
    return false;
}

// Given x[] and y[] writes out such k and b that line y=k*x+b approximates
// given points in the best way (Least Squares Method).
bool RtpToNtpEstimator::LinearRegression(double* k, double* b) const {
    size_t n = measurements_.size();
    if (n < 2) {
        PLOG_WARNING << "The method needs at least 2 RTP/NTP timestamp pairs to calculate linear regression parameters.";
        return false;
    }

    double avg_x = 0;
    double avg_y = 0;
    for (const auto& measurement : measurements_) {
      avg_x += double(measurement.unwrapped_rtp_timestamp);
      avg_y += double(measurement.ntp_time_ms);
    }
    avg_x /= n;
    avg_y /= n;

    double variance_x = 0;
    double covariance_xy = 0;
    for (const auto& measurement : measurements_) {
        double normalized_x = double(measurement.unwrapped_rtp_timestamp) - avg_x;
        double normalized_y = double(measurement.ntp_time_ms) - avg_y;
        variance_x += normalized_x * normalized_x;
        covariance_xy += normalized_x * normalized_y;
    }

    // The change of rtp timestamp is too small or zero, drop it.
    if (std::fabs(variance_x) < 1e-8)
        return false;

    *k = static_cast<double>(covariance_xy / variance_x);
    *b = static_cast<double>(avg_y - (*k) * avg_x);
    return true;
}
    
} // namespace naivertc
