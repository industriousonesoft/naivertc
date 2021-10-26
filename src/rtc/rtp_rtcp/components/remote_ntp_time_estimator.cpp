#include "rtc/rtp_rtcp/components/remote_ntp_time_estimator.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
constexpr int kMinimumNumberOfSamples = 2;
constexpr int kTimingLogIntervalMs = 10000; // 10s
constexpr int kClocksOffsetSmoothingWindow = 100;
}  // namespace

RemoteNtpTimeEstimator::RemoteNtpTimeEstimator(std::shared_ptr<Clock> clock) 
    : clock_(std::move(clock)),
      ntp_clocks_offset_estimator_(kClocksOffsetSmoothingWindow),
      last_timing_log_ms_(-1) {}

RemoteNtpTimeEstimator::~RemoteNtpTimeEstimator() {}

bool RemoteNtpTimeEstimator::UpdateTimestamp(int64_t rtt, 
                                             uint32_t ntp_secs, 
                                             uint32_t ntp_frac, 
                                             uint32_t rtp_timestamp) {
    if (!rtp_to_ntp_estimator_.UpdateMeasurements(ntp_secs, ntp_frac, rtp_timestamp)) {
        return false;
    }
    int64_t receiver_arrival_time_ms = clock_->CurrentNtpTime().ToMs();
    int64_t sender_send_time_ms = NtpTime(ntp_secs, ntp_frac).ToMs();
    int64_t sender_arrival_time_ms = sender_send_time_ms + rtt / 2;
    int64_t remote_to_local_clocks_offset = receiver_arrival_time_ms - sender_arrival_time_ms;
    ntp_clocks_offset_estimator_.Insert(remote_to_local_clocks_offset);
    return true;
}

int64_t RemoteNtpTimeEstimator::Estimate(uint32_t rtp_timestamp) {
    auto sender_capture_ntp_ms = rtp_to_ntp_estimator_.Estimate(rtp_timestamp);
    if (!sender_capture_ntp_ms) {
        return -1;
    }
    int64_t remote_to_local_clocks_offset = ntp_clocks_offset_estimator_.GetFilteredValue();
    int64_t receiver_capture_ntp_ms = sender_capture_ntp_ms.value() + remote_to_local_clocks_offset;

    int64_t now_ms = clock_->now_ms();
    if (now_ms - last_timing_log_ms_ > kTimingLogIntervalMs) {
        PLOG_INFO << "RTP timestamp: " << rtp_timestamp
                  << " in NTP clock: " << sender_capture_ntp_ms.value()
                  << " estimated time in receiver NTP clock: "
                  << receiver_capture_ntp_ms;
        last_timing_log_ms_ = now_ms;
    }

    return receiver_capture_ntp_ms;
}

std::optional<int64_t> RemoteNtpTimeEstimator::EstimateRemoteToLocalClockOffsetMs() {
    if (ntp_clocks_offset_estimator_.stored_sample_count() < kMinimumNumberOfSamples) {
        return std::nullopt;
    }
    return ntp_clocks_offset_estimator_.GetFilteredValue();
}
    
} // namespace naivertc
