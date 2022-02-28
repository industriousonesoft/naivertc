#include "rtc/congestion_controller/goog_cc/congestion_window_pushback_controller.hpp"

namespace naivertc {

CongestionWindwoPushbackController::CongestionWindwoPushbackController(Configuration config) 
    : config_(config),
      congestion_window_(config.initial_congestion_window) {}

CongestionWindwoPushbackController::~CongestionWindwoPushbackController() {}

void CongestionWindwoPushbackController::set_congestion_window(size_t congestion_window) {
    congestion_window_ = congestion_window;
}

void CongestionWindwoPushbackController::OnOutstandingBytes(int64_t outstanding_bytes) {
    outstanding_bytes_ = outstanding_bytes;
}

void CongestionWindwoPushbackController::OnPacingQueue(int64_t pacing_bytes) {
    pacing_bytes_ = pacing_bytes;
}

DataRate CongestionWindwoPushbackController::AdjustTargetBitrate(DataRate target_bitrate) {
    if (congestion_window_ == 0) {
        return target_bitrate;
    }
    int64_t total_inflight_bytes = outstanding_bytes_;
    // Append the bytes in pacing queue if using pacing.
    if (config_.use_pacing) {
        total_inflight_bytes += pacing_bytes_;
    }
    double fill_ratio = total_inflight_bytes / static_cast<double>(congestion_window_);
    if (fill_ratio > 1.5) {
        encoding_bitrate_ratio_ *= 0.9;
    } else if (fill_ratio > 1.0) {
        encoding_bitrate_ratio_ *= 0.95;
    } else if (fill_ratio < 0.1) {
        encoding_bitrate_ratio_ = 1.0;
    } else {
        // fill ratio in range: [0.1, 1.0]
        encoding_bitrate_ratio_ *= 1.05;
        encoding_bitrate_ratio_ = std::min(encoding_bitrate_ratio_, 1.0);
    }
    auto adjust_target_bitrate = target_bitrate * encoding_bitrate_ratio_;

    // Do not adjust below the minimum pushback bitrate, but
    // do obey if the original target bitrate is below it.
    return adjust_target_bitrate < config_.min_pushback_bitrate ? std::min(target_bitrate, config_.min_pushback_bitrate)
                                                                : adjust_target_bitrate;
}
    
} // namespace naivertc
