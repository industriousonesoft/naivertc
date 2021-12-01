#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_TIMING_TIMING_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_TIMING_TIMING_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timestamp_extrapolator.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/decode_time_filter.hpp"

#include <memory>

namespace naivertc {
namespace rtp {
namespace video {

// The class is not thread-safe, the caller MUST privode that.
class RTC_CPP_EXPORT Timing {
public:
    struct TimingInfo {
        int max_decode_ms = -1;
        int curr_playout_delay_ms = -1;
        int target_delay_ms = -1;
        int jitter_delay_ms = -1;
        int min_playout_delay_ms = -1;
        int render_delay_ms = -1;
    };
public:
    explicit Timing(Clock* clock);
    virtual ~Timing();

    int min_playout_delay_ms() const { return min_playout_delay_ms_; }
    void set_min_playout_delay_ms(int delay_ms) { min_playout_delay_ms_ = delay_ms; }

    int max_playout_delay_ms() const { return max_playout_delay_ms_; }
    void set_max_playout_delay_ms(int delay_ms) { max_playout_delay_ms_ = delay_ms; }

    // Set the amount of time needed to render an image. Defaults to 10 ms.
    void set_render_delay_ms(int delay_ms) { render_delay_ms_ = delay_ms; }

    // Set the minimum time the video must be delayed on the receiver to
    // get the desired jitter buffer level.
    void set_jitter_delay_ms(int delay_ms);

    void set_zero_playout_delay_min_pacing(TimeDelta time_delta) { zero_playout_delay_min_pacing_ = time_delta; }
    TimeDelta zero_playout_delay_min_pacing() const { return zero_playout_delay_min_pacing_; }

    // Used to report that a frame is passed to decoding. Updates the timestamp
    // filter which is used to map between timestamps and receiver system time.
    void IncomingTimestamp(uint32_t timestamp, int64_t receive_time_ms);

    void AddDecodeTime(int32_t decode_time_ms, int64_t now_ms);

    // Increases or decreases the current delay to get closer to the target delay.
    // Calculates how long it has been since the previous call to thie function,
    // and increases or decreases the delay in proportion to the time difference.
    void UpdateCurrentDelay(uint32_t timestamp);

    // Increases or decreases the current delay to get closer to the target delay.
    // Given the actual time to decode in ms and the render time in ms for a frame,
    // this function calculates how late the frame is and increases the delay
    // accordingly.
    void UpdateCurrentDelay(int64_t render_time_ms,
                            int64_t actual_decode_time_ms);
    
    // Returns the current target delay which is required delay + decode time +
    // render delay.
    int TargetDelayMs() const;

    // Returns the receiver system time when the frame with timestamp
    // |frame_timestamp| should be rendered, assuming that the system time
    // currently is |now_ms|.
    virtual int64_t RenderTimeMs(uint32_t timestamp, int64_t now_ms) const;

    // Returns the maximum time in ms that we can wait for a frame to become
    // complete before we must pass it to the decoder.
    virtual int64_t MaxWaitingTimeBeforeDecode(int64_t render_time_ms, int64_t now_ms);

    // Return current timing information. Returns true if the first frame has been
    // decoded, false otherwise.
    virtual std::pair<TimingInfo, bool> GetTimingInfo() const;

    // Reset the timing to the initial state.
    void Reset();

private:
    int RequiredDecodeTimeMs() const;
private:
    Clock* const clock_;
    std::unique_ptr<TimestampExtrapolator> timestamp_extrapolator_;
    std::unique_ptr<DecodeTimeFilter> decode_time_filter_;

    // Indicates if the low-latency renderer algorithm should be 
    // used for the case min playout delay=0 and max playout delay > 0.
    const bool low_latency_renderer_enabled_;

    // Indicates the minimum delay between frames scheduled for decoding 
    // that is used when min playout delay=0 and max playout delay>=0
    TimeDelta zero_playout_delay_min_pacing_;

    int render_delay_ms_;
    // Best-effort playout delay range for frames from capture to render.
    // The receiver tries to keep the delay between |min_playout_delay_ms_|
    // and |max_playout_delay_ms_| taking the network jitter into account.
    // A special case is where min_playout_delay_ms_ = max_playout_delay_ms_ = 0,
    // in which case the receiver tries to play the frames as they arrive.
    int min_playout_delay_ms_;
    int max_playout_delay_ms_;
    int jitter_delay_ms_;
    int curr_playout_delay_ms_;
    uint32_t prev_timestamp_;
    size_t num_decoded_frames_;

    // An estimate of when the last frame is scheduled to be sent to the decoder.
    // Used only when the RTP header extension playout delay is set to min=0 ms
    // which is indicated by a render time set to 0.
    int64_t earliest_next_decode_start_time_;
    
};
    
} // namespace video
} // namespace rtp
} // namespace naivertc


#endif