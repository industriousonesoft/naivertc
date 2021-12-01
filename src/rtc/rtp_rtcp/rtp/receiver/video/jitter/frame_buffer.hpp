#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_BUFFER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_BUFFER_H_

#include "base/defines.hpp"
#include "base/thread_annotation.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/inter_frame_delay.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/decoded_frames_history.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_defines.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_estimator.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"
#include "rtc/base/task_utils/repeating_task.hpp"

#include <optional>
#include <map>
#include <vector>
#include <mutex>

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {

// The class is not thread-safe, the caller MUST privode that.
class RTC_CPP_EXPORT FrameBuffer final {
public:
    enum class ReturnReason { FOUND, TIME_OUT, STOPPED };
public:
    FrameBuffer(Clock* clock, 
                Timing* timing,
                TaskQueue* decode_queue,
                VideoReceiveStatisticsObserver* stats_observer);
    ~FrameBuffer();

    ProtectionMode protection_mode() const;
    void set_protection_mode(ProtectionMode mode) RTC_LOCKS_EXCLUDED(lock_);
    
    void UpdateRtt(int64_t rtt_ms) RTC_LOCKS_EXCLUDED(lock_);

    std::pair<int64_t, bool> InsertFrame(video::FrameToDecode frame) RTC_LOCKS_EXCLUDED(lock_);

    using NextFrameFoundCallback = std::function<void(std::optional<video::FrameToDecode>)>;
    void NextFrame(int64_t max_wait_time_ms, 
                   bool keyframe_required,
                   NextFrameFoundCallback callback) RTC_LOCKS_EXCLUDED(lock_);

    void Clear() RTC_LOCKS_EXCLUDED(lock_);

private:
    struct FrameInfo {
        FrameInfo();
        ~FrameInfo();

        int64_t frame_id() const { return frame.has_value() ? frame->id() : -1; }

        // Indicate if the frame is continuous or not.
        bool continuous() const { return num_missing_continuous == 0; }

        // A frame is continuous if it has all its referenced/indirectly referenced frames.
        // Indicate how many unfulfilled frames this frame have until it becomes continuous.
        size_t num_missing_continuous = 0;

        // A frame is decodable if it has all its referenced frames have been decoded.
        // Indicate how many unfulfilled frames this frame have until it becomes decodable.
        size_t num_missing_decodable = 0;

        // Which other frames that have direct unfulfilled dependencies on this frame.
        std::vector<int64_t> dependent_frames;

        std::optional<video::FrameToDecode> frame = std::nullopt;
    };
    // FIXME: Is it necessary to add a compare to sort map by frame id?
    // DONE: No, because the frame id was unwrapped and will be sorted (ascending).
    using FrameInfoMap = std::map<int64_t, FrameInfo>;

private:
    void ClearFramesAndHistory() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
    size_t NumUndecodableFrames(FrameInfoMap::iterator begin, FrameInfoMap::iterator end) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
    int EstimateJitterDelay(uint32_t send_timestamp, int64_t recv_time_ms, size_t frame_size) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

    // Continuity
    bool ValidReferences(const video::FrameToDecode& frame) const;
    // Returns a pair consisting of an iterator to the inserted element,
    // or the already-existing element if no insertion happened,
    // and a bool denoting whether the insertion took place(true if insertion
    // happened, false if it did not).
    std::pair<FrameInfo&, bool> EmplaceFrameInfo(video::FrameToDecode frame) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
    void PropagateContinuity(const FrameInfo& frame_info) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

    // Decodability
    void FindNextDecodableFrames(int64_t last_decodable_frame_id) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
    bool IsValidRenderTiming(int64_t render_time_ms, int64_t now_ms) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
    int64_t PropagateDecodability(const FrameInfo& frame_info) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
    void StartWaitForNextFrameToDecode() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
    int64_t FindNextFrameToDecode() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
    video::FrameToDecode GetNextFrameToDecode() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

private:
    static constexpr int64_t kLogNonDecodedIntervalMs = 5000;
private:
    std::mutex lock_;
    Clock* const clock_ RTC_GUARDED_BY(lock_);
    Timing* const timing_ RTC_GUARDED_BY(lock_);
    TaskQueue* const decode_queue_ RTC_GUARDED_BY(lock_);
    VideoReceiveStatisticsObserver* const stats_observer_;

    InterFrameDelay inter_frame_delay_ RTC_GUARDED_BY(lock_);
    DecodedFramesHistory decoded_frames_history_ RTC_GUARDED_BY(lock_);
    JitterEstimator jitter_estimator_ RTC_GUARDED_BY(lock_);
    ProtectionMode protection_mode_ RTC_GUARDED_BY(lock_);
    const bool add_rtt_to_playout_delay_;
    int64_t last_log_non_decoded_ms_ RTC_GUARDED_BY(lock_);

    std::optional<int64_t> last_continuous_frame_id_ RTC_GUARDED_BY(lock_) = std::nullopt;
    
    FrameInfoMap frame_infos_ RTC_GUARDED_BY(lock_);
    std::unique_ptr<RepeatingTask> decode_task_ RTC_GUARDED_BY(lock_) = nullptr;
    std::optional<FrameInfoMap::iterator> frame_to_decode_;
    bool keyframe_required_ RTC_GUARDED_BY(lock_) = false;
    int64_t waiting_deadline_ms_ RTC_GUARDED_BY(lock_) = 0;
    NextFrameFoundCallback next_frame_found_callback_ RTC_GUARDED_BY(lock_) = nullptr;
};
    
} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc

#endif