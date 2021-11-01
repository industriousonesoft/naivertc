#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp"
#include "rtc/base/numerics/modulo_operator.hpp"

namespace naivertc {
namespace rtc {
namespace video {
namespace {

constexpr int kDefaultRenderDelayMs = 10;
constexpr int kDelayMaxChangeMsPerS = 100;

} // namespace


Timing::Timing(std::shared_ptr<Clock> clock) 
    : clock_(std::move(clock)),
      timestamp_extrapolator_(nullptr),
      decode_time_filter_(std::make_unique<DecodeTimeFilter>()),
      low_latency_renderer_enabled_(true),
      zero_playout_delay_min_pacing_(TimeDelta::Millis(0)),
      render_delay_ms_(kDefaultRenderDelayMs),
      min_playout_delay_ms_(0),
      max_playout_delay_ms_(10000 /* 10s */),
      jitter_delay_ms_(0),
      curr_delay_ms_(0),
      prev_timestamp_(0),
      num_decoded_frames_(0),
      earliest_next_decode_start_time_(0) {
    assert(clock_ != nullptr);
    timestamp_extrapolator_ = std::make_unique<TimestampExtrapolator>(clock_->now_ms());
}

Timing::~Timing() {}

void Timing::Reset() {
    timestamp_extrapolator_->Reset(clock_->now_ms());
    decode_time_filter_->Reset();
    render_delay_ms_ = kDefaultRenderDelayMs;
    min_playout_delay_ms_ = 0;
    jitter_delay_ms_ = 0;
    curr_delay_ms_ = 0;
    prev_timestamp_ = 0;
}

void Timing::IncomingTimestamp(uint32_t timestamp, int64_t receive_time_ms) {
    timestamp_extrapolator_->Update(timestamp, receive_time_ms);
}

void Timing::AddDecodeTime(int32_t decode_time_ms, int64_t now_ms) {
    assert(decode_time_ms >= 0);
    decode_time_filter_->AddTiming(decode_time_ms, now_ms);
    ++num_decoded_frames_;
}

void Timing::set_jitter_delay_ms(int delay_ms) {
    if (jitter_delay_ms_ != delay_ms) {
        jitter_delay_ms_ = delay_ms;
        // Set the current delay to minimum delay,
        // when in initial state.
        if (curr_delay_ms_ == 0) {
            curr_delay_ms_ = jitter_delay_ms_;
        }
    }
}

void Timing::UpdateCurrentDelay(uint32_t timestamp) {
    int target_delay_ms = TargetDelayMs();

    if (curr_delay_ms_ == 0) {
        curr_delay_ms_ = target_delay_ms;
    } else if (target_delay_ms != curr_delay_ms_) {
        int64_t delay_diff_ms = static_cast<int64_t>(target_delay_ms) - curr_delay_ms_;

        // Never change the delay with more than 100ms every second. If we're
        // changing the delay in too large steps we will get noticeable freezes.
        // By limiting the change we can increase the delay in smaller steps, which
        // will be experienced as the video is played in slow motion. when lowering
        // the delay the video will be played at a faster pace. 
        int64_t max_change_ms = 0;
        // Calculate the forward from previous timestamp to current timestamp,
        // and take wraparound into account.
        // TODO: Take multiple wraparound into account, add `num_wrap_arounds` used in TimestampExtrapolator? 
        int64_t timestamp_diff = static_cast<int64_t>(ForwardDiff<uint32_t>(prev_timestamp_, timestamp));
        max_change_ms = kDelayMaxChangeMsPerS * (timestamp_diff / 90000 /* Elapsed time in seconds */);

        if (max_change_ms <= 0) {
            // Any changes less than 1 ms are truncated and will be postponed.
            // Negative change will be due to reordering and should be ignored.
            return;
        }
        // Limits `delay_diff_ms` in range: [-max_change_ms, max_change_ms]
        delay_diff_ms = std::max(delay_diff_ms, -max_change_ms);
        delay_diff_ms = std::min(delay_diff_ms, max_change_ms);

        curr_delay_ms_ += delay_diff_ms;
    }
    prev_timestamp_ = timestamp;
    
}

void Timing::UpdateCurrentDelay(int64_t expect_render_time_ms,
                                int64_t actual_time_ms_to_decode) {
    uint32_t target_delay_ms = TargetDelayMs();
    // The time consumed by video decoder.
    int decode_time_ms = RequiredDecodeTimeMs();
    // The time expected to start to decode.
    int64_t expect_time_ms_to_decode = expect_render_time_ms - decode_time_ms - render_delay_ms_;

    int64_t decode_delayed_ms = actual_time_ms_to_decode - expect_time_ms_to_decode;
    if (decode_delayed_ms < 0) {
        return;
    }
    // Closing to the target delay, but not be greater than it.
    if (curr_delay_ms_ + decode_delayed_ms <= target_delay_ms) {
        curr_delay_ms_ += decode_delayed_ms;
    } else {
        // The upper limit of delay is `target_delay_ms`.
        curr_delay_ms_ = target_delay_ms;
    }
}

int Timing::TargetDelayMs() const {
    return std::max(min_playout_delay_ms_, jitter_delay_ms_ + RequiredDecodeTimeMs() + render_delay_ms_);
}

std::pair<Timing::TimingInfo, bool> Timing::GetTimingInfo() const {
    TimingInfo info;
    info.max_decode_ms = RequiredDecodeTimeMs();
    info.curr_delay_ms = curr_delay_ms_;
    info.target_delay_ms = TargetDelayMs();
    info.jitter_delay_ms = jitter_delay_ms_;
    info.min_playout_delay_ms = min_playout_delay_ms_;
    info.render_delay_ms = render_delay_ms_;
    return {std::move(info), (num_decoded_frames_ > 0)};
}

int64_t Timing::RenderTimeMs(uint32_t timestamp, int64_t now_ms) const {
    constexpr int kLowLatencyRenderMaxPlayoutDelayMs = 500;
    // Render as soon as possible or with low-latency renderer algorith.
    if (min_playout_delay_ms_ == 0 && 
        (max_playout_delay_ms_ == 0 || 
        (low_latency_renderer_enabled_ && max_playout_delay_ms_ <= kLowLatencyRenderMaxPlayoutDelayMs))) {
        return 0;
    }

    int64_t estimated_complete_time_ms = timestamp_extrapolator_->ExtrapolateLocalTime(timestamp);
    if (estimated_complete_time_ms == -1) {
        estimated_complete_time_ms = now_ms;
    }
    // Make sure the actual delay limits in the range of `min_playout_delay_ms_` and `max_playout_delay_ms_`.
    int actual_delay = std::max(curr_delay_ms_, min_playout_delay_ms_);
    actual_delay = std::min(actual_delay, max_playout_delay_ms_);

    return estimated_complete_time_ms + actual_delay;
}

int64_t Timing::MaxTimeWaitingToDecode(int64_t render_time_ms, int64_t now_ms) {
    if (render_time_ms == 0 && zero_playout_delay_min_pacing_.us() > 0) {
        // `render_time_ms` == 0 indicates that the frame should be decoded
        // and rendered ASAP. However, the decoder can be choked if too many
        // frames are sent at ones. Therefore, limit the interframe delay to
        // `zero_playout_delay_min_pacing_`.
        int64_t max_wait_time_ms = now_ms >= earliest_next_decode_start_time_
                                      ? 0
                                      : earliest_next_decode_start_time_ - now_ms;
        earliest_next_decode_start_time_ = now_ms + max_wait_time_ms + zero_playout_delay_min_pacing_.ms();
        return max_wait_time_ms;
    }
    return render_time_ms - now_ms - RequiredDecodeTimeMs() - render_delay_ms_;
}

// Private methods
int Timing::RequiredDecodeTimeMs() const {
    const int decode_time_ms = decode_time_filter_->RequiredDecodeTimeMs();
    assert(decode_time_ms >= 0);
    return decode_time_ms;
}

} // namespace video
} // namespace rtc
} // namespace naivertc