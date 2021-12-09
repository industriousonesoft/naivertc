#include "rtc/congestion_controller/goog_cc/bitrate_estimator.hpp"

namespace naivertc {
namespace {

constexpr int kMinRateWindowMs = 150;
constexpr int kMaxRateWindowMs = 1000;
    
} // namespac 

BitrateEstimator::BitrateEstimator(Configuration config) 
    : config_(std::move(config)),
      accumulated_bytes_(0),
      curr_window_ms_(0),
      prev_time_ms_(std::nullopt),
      bitrate_estimate_kbps_(-1.0f),
      bitrate_estimate_var_(50.f) {
    // MAKE SURE the `initial_window_ms` and `noninitial_window_ms` is limited in range [150, 1000].
    assert(config_.initial_window_ms >= kMinRateWindowMs && 
           config_.initial_window_ms <= kMaxRateWindowMs);
    assert(config_.noninitial_window_ms >= kMinRateWindowMs && 
           config_.noninitial_window_ms <= kMaxRateWindowMs);
}

BitrateEstimator::~BitrateEstimator() = default;

void BitrateEstimator::Update(Timestamp at_time, size_t amount, bool in_alr) {
    int rate_window_ms = config_.noninitial_window_ms;
    // We use a larger window at the beginning to get a more
    // stable sample that we can use to initialize the estimate.
    if (bitrate_estimate_kbps_ < 0.f) {
        rate_window_ms = config_.initial_window_ms;
    }
    
    auto [immediate_bitrate_kbps, is_small_sample] = CalcImmediateBitrate(at_time.ms(), amount, rate_window_ms);
    if (immediate_bitrate_kbps < 0.0f) { 
        return;
    }
    if (bitrate_estimate_kbps_ < 0.0f) {
        // This is the very first sample we get. Use it to initialize the estimate.
        bitrate_estimate_kbps_ = immediate_bitrate_kbps;
        return;
    }
    // Optionally use higher uncertainly for very small samples to avoid dropping
    // estimate and for samples obtained in ALR.
    float scale = config_.uncertainty_scale;
    if (is_small_sample && immediate_bitrate_kbps < bitrate_estimate_kbps_) {
        scale = config_.small_sample_uncertainty_scale;
    } else if (in_alr && immediate_bitrate_kbps < bitrate_estimate_kbps_) {
        // Optionally use higher uncertainty for samples obtained during ALR.
        scale = config_.uncertainty_scale_in_alr;
    }
    // Define the sample uncertainty as a function of how far away it is from the
    // current estimate. With low values of uncertainty_symmetry_cap we add more
    // uncertainty to increases than to decreases. For higher values we approach
    // symmetry.
    float sample_uncertainty = scale * std::abs(bitrate_estimate_kbps_ - immediate_bitrate_kbps) /
                               (bitrate_estimate_kbps_ + std::min(immediate_bitrate_kbps, config_.uncertainty_symmetry_cap.kbps<float>()));
    
    float sample_var = sample_uncertainty * sample_uncertainty;
    // Update a bayesian estimate of the rate, weighting it lower if the sample
    // uncertainty is large.
    // The bitrate estimate uncertainty is increased with each update to model
    // that the bitrate changes over time.
    // FIXME: How to understand the formula below?
    float pred_bitrate_estimate_var = bitrate_estimate_var_ + 5.f;
    bitrate_estimate_kbps_ = (sample_var * bitrate_estimate_kbps_ +
                              pred_bitrate_estimate_var * immediate_bitrate_kbps) /
                              (sample_var + pred_bitrate_estimate_var);
    bitrate_estimate_kbps_ = std::max(bitrate_estimate_kbps_, config_.estimate_floor.kbps<float>());
    bitrate_estimate_var_ = sample_var * pred_bitrate_estimate_var / 
                            (sample_var + pred_bitrate_estimate_var);

}   

std::optional<DataRate> BitrateEstimator::Estimate() const {
    if (bitrate_estimate_kbps_ >= 0.0f) {
        return DataRate::KilobitsPerSec(bitrate_estimate_kbps_);
    }
    return std::nullopt;
}

std::optional<DataRate> BitrateEstimator::PeekRate() const {
    if (curr_window_ms_ > 0) {
        return DataRate::BytesPerSec(accumulated_bytes_ * 1000 / curr_window_ms_);
    }
    return std::nullopt;
}

void BitrateEstimator::ExpectFastRateChange() {
    // By setting the bitrate-estimate variance to a higher value we allow the
    // bitrate to change fast for the next few samples.
    bitrate_estimate_var_ += 200;
}

// Private methods
std::pair<float, bool> BitrateEstimator::CalcImmediateBitrate(int64_t now_ms,
                                                              int bytes,
                                                              int rate_window_ms) {
    if (prev_time_ms_) {
        // Reset if time moves backwards.
        if (now_ms < *prev_time_ms_) {
            prev_time_ms_.reset();
            accumulated_bytes_ = 0;
            curr_window_ms_ = 0;
        } else {
            int elapsed_time_ms = static_cast<int>(now_ms - *prev_time_ms_);
            curr_window_ms_ += elapsed_time_ms;
            // Reset if nothing has been received for more than a full window.
            if (elapsed_time_ms > rate_window_ms) {
                accumulated_bytes_ = 0;
                curr_window_ms_ %= rate_window_ms;
            }
        }
    }
    prev_time_ms_ = now_ms;
    float immediate_bitrate_kbps = -1.0f;
    bool is_small_sample = false;
    // Calculate the immediate bitrate every full window. 
    if (curr_window_ms_ >= rate_window_ms) {
        is_small_sample = accumulated_bytes_ < config_.small_sample_threshold;
        immediate_bitrate_kbps = 8.0f * accumulated_bytes_ / static_cast<float>(rate_window_ms);
        curr_window_ms_ -= rate_window_ms;
        accumulated_bytes_ = 0;
    }
    accumulated_bytes_ += bytes;
    return {immediate_bitrate_kbps, is_small_sample};
}
    
} // namespace naivertc
