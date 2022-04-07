#include "rtc/congestion_control/send_side/goog_cc/congestion_window_pushback_controller.hpp"

namespace naivertc {
namespace {

constexpr double kInitialEncodingBitrateRatio = 1.0;

} // namespace

CongestionWindwoPushbackController::CongestionWindwoPushbackController(const Configuration& config) 
    : add_pacing_(config.add_pacing),
      min_pushback_bitrate_(config.min_pushback_bitrate),
      congestion_window_(config.initial_congestion_window),
      inflight_bytes_(0),
      pacing_bytes_(0),
      encoding_bitrate_ratio_(kInitialEncodingBitrateRatio) {}

CongestionWindwoPushbackController::~CongestionWindwoPushbackController() {}

void CongestionWindwoPushbackController::set_congestion_window(size_t congestion_window) {
    congestion_window_ = congestion_window;
}

void CongestionWindwoPushbackController::OnInflightBytes(int64_t inflight_bytes) {
    inflight_bytes_ = inflight_bytes;
}

void CongestionWindwoPushbackController::OnPacingQueueSize(int64_t pacing_bytes) {
    pacing_bytes_ = pacing_bytes;
}

DataRate CongestionWindwoPushbackController::AdjustTargetBitrate(DataRate target_bitrate) {
    if (congestion_window_ == 0) {
        return target_bitrate;
    }
    int64_t total_inflight_bytes = inflight_bytes_;
    // Append the bytes in pacing queue if using pacing.
    if (add_pacing_) {
        total_inflight_bytes += pacing_bytes_;
    }
    double fill_ratio = total_inflight_bytes / static_cast<double>(congestion_window_);
    if (fill_ratio > 1.5) {
        encoding_bitrate_ratio_ *= 0.9;
    } else if (fill_ratio > 1.0) {
        encoding_bitrate_ratio_ *= 0.95;
    } else if (fill_ratio < 0.1) {
        // Reset
        encoding_bitrate_ratio_ = kInitialEncodingBitrateRatio;
    } else {
        // fill ratio in range: [0.1, 1.0]
        // Recover from decrease.
        encoding_bitrate_ratio_ *= 1.05;
        // Make sure the recovered ratio not exceeds the initial value.
        encoding_bitrate_ratio_ = std::min(encoding_bitrate_ratio_, kInitialEncodingBitrateRatio);
    }
    auto adjust_target_bitrate = target_bitrate * encoding_bitrate_ratio_;

    // Do not adjust below the minimum pushback bitrate, but
    // do obey if the original target bitrate is below it.
    return adjust_target_bitrate < min_pushback_bitrate_ ? std::min(target_bitrate, min_pushback_bitrate_)
                                                         : adjust_target_bitrate;
}
    
} // namespace naivertc
