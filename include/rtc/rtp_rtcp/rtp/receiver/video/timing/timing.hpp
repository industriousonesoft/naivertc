#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_TIMING_TIMING_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_TIMING_TIMING_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timestamp_extrapolator.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/decode_time_filter.hpp"

#include <memory>

namespace naivertc {
namespace rtc {
namespace video {

// The class is not thread-safe, the caller MUST privode that.
class RTC_CPP_EXPORT Timing {
public:
    explicit Timing(std::shared_ptr<Clock> clock);
    ~Timing();

    int min_playout_delay_ms() const { return min_playout_delay_ms_; }
    void set_min_playout_delay_ms(int delay_ms) { min_playout_delay_ms_ = delay_ms; }

    int max_playout_delay_ms() const { return max_playout_delay_ms_; }
    void set_max_playout_delay_ms(int delay_ms) { max_playout_delay_ms_ = delay_ms; }

    // Set the amount of time needed to render an image. Defaults to 10 ms.
    void set_render_delay_ms(int delay_ms) { render_delay_ms_ = delay_ms; }

    // Set the minimum time the video must be delayed on the receiver to
    // get the desired jitter buffer level.
    void HitJitterDelayMs(int delay_ms);

    void IncomingTimestamp(uint32_t timestamp, int64_t receive_time_ms);

    // Increases or decreases the current delay to get closer to the target delay.
    // Calculates how long it has been since the previous call to thie function,
    // and increases or decreases the delay in proportion to the time difference.
    void UpdateCurrentDelay(uint32_t timestamp);

    // Increases or decreases the current delay to get closer to the target delay.
    // Given the actual time to decode in ms and the render time in ms for a frame,
    // this function calculates how late the frame is and increases the delay
    // accordingly.
    void UpdateCurrentDelay(int64_t expect_render_time_ms,
                            int64_t actual_time_ms_to_decode);
    
    void Reset();

    int TargetDelayMs() const;

    bool GetTiming(int* max_decode_ms,
                   int* curr_delay_ms,
                   int* target_delay_ms,
                   int* jitter_delay_ms,
                   int* min_playout_delay_ms,
                   int* render_delay_ms) const;

private:

private:
    std::shared_ptr<Clock> clock_;
    std::unique_ptr<TimestampExtrapolator> timestamp_extrapolator_;
    std::unique_ptr<DecodeTimeFilter> decode_time_filter_;

    // Indicates if the low-latency renderer algorithm should be 
    // used for the case min playout delay=0 and max playout delay > 0.
    const bool low_latency_render_enable_;

    // Indicates the minimum delay between frames scheduled for decoding 
    // that is used when min playout delay=0 and max playout delay>=0
    const TimeDelta zero_playout_delay_min_pacing_;

    int render_delay_ms_;
    // Best-effort playout delay range for frames from capture to render.
    // The receiver tries to keep the delay between |min_playout_delay_ms_|
    // and |max_playout_delay_ms_| taking the network jitter into account.
    // A special case is where min_playout_delay_ms_ = max_playout_delay_ms_ = 0,
    // in which case the receiver tries to play the frames as they arrive.
    int min_playout_delay_ms_;
    int max_playout_delay_ms_;
    int jitter_delay_ms_;
    int curr_delay_ms_;
    uint32_t prev_timestamp_;
    size_t num_decoded_frames_;

    // An estimate of when the last frame is scheduled to be sent to the decoder.
    // Used only when the RTP header extension playout delay is set to min=0 ms
    // which is indicated by a render time set to 0.
    int64_t earliest_next_decode_start_time_;
    
};
    
} // namespace video
} // namespace rtc
} // namespace naivertc


#endif