#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_BUFFER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_BUFFER_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/inter_frame_delay.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/decoded_frames_history.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_defines.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_estimator.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"
#include "rtc/base/repeating_task.hpp"

#include <optional>
#include <map>
#include <vector>
#include <list>

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {

// The class is not thread-safe, the caller MUST privode that.
class RTC_CPP_EXPORT FrameBuffer {
public:
    enum class ReturnReason { FOUND, TIME_OUT, STOPPED };
public:
    FrameBuffer(ProtectionMode protection_mode,
                std::shared_ptr<Clock> clock, 
                std::shared_ptr<Timing> timing,
                std::shared_ptr<TaskQueue> task_queue,
                std::shared_ptr<TaskQueue> decode_queue,
                std::weak_ptr<VideoReceiveStatisticsObserver> stats_observer);
    ~FrameBuffer();

    ProtectionMode protection_mode() const;
    
    void UpdateRtt(int64_t rtt_ms);

    std::pair<int64_t, bool> InsertFrame(video::FrameToDecode frame);

    using NextFrameFoundCallback = std::function<void(std::optional<video::FrameToDecode>)>;
    void NextFrame(int64_t max_wait_time_ms, 
                   bool keyframe_required, 
                   NextFrameFoundCallback callback);

    void Clear();

private:
    struct FrameInfo {
        FrameInfo(video::FrameToDecode frame);
        ~FrameInfo();

        int64_t frame_id() const { return IsValid() ? frame.id() : -1; }

        bool IsValid() const { return frame.cdata() != nullptr; }

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

        video::FrameToDecode frame;
    };
    // FIXME: Is it necessary to add a compare to sort map by frame id?
    // DONE: No, because the frame id was unwrapped and will be sorted (ascending).
    using FrameInfoMap = std::map<int64_t, FrameInfo>;

private:
    size_t NumUndecodedFrames(FrameInfoMap::iterator begin, FrameInfoMap::iterator end);
    int EstimateJitterDelay(uint32_t send_timestamp, int64_t recv_time_ms, size_t frame_size);

    // Continuity
    std::pair<int64_t, bool> InsertFrameIntrenal(video::FrameToDecode frame);
    bool ValidReferences(const video::FrameToDecode& frame);
    // Returns a pair consisting of an iterator to the inserted element,
    // or the already-existing element if no insertion happened,
    // and a bool denoting whether the insertion took place(true if insertion
    // happened, false if it did not).
    std::pair<FrameInfoMap::iterator, bool> EmplaceFrameInfo(video::FrameToDecode frame);
    void PropagateContinuity(const FrameInfo& frame_info);

    // Decodability
    void FindNextDecodableFrames();
    bool IsValidRenderTiming(int64_t render_time_ms, int64_t now_ms);
    bool PropagateDecodability(const FrameInfo& frame_info);
    void StartWaitForNextFrameToDecode();
    int64_t FindNextFrameToDecode();
    void DeliverFrameToDecode(video::FrameToDecode frame);
    void OnDecodedFrame(int64_t frame_id, uint32_t timestamp);

private:
    static constexpr int64_t kLogNonDecodedIntervalMs = 5000;
private:
    const ProtectionMode protection_mode_;
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<Timing> timing_;
    std::shared_ptr<TaskQueue> task_queue_;
    std::shared_ptr<TaskQueue> decode_queue_;
    std::weak_ptr<VideoReceiveStatisticsObserver> stats_observer_;

    InterFrameDelay inter_frame_delay_;
    DecodedFramesHistory decoded_frames_history_;
    JitterEstimator jitter_estimator_;
    const bool add_rtt_to_playout_delay_;
    int64_t last_log_non_decoded_ms_;

    std::optional<int64_t> last_continuous_frame_id_ = std::nullopt;
    std::optional<int64_t> last_decodable_frame_id_ = std::nullopt;

    FrameInfoMap frame_infos_;
    std::list<video::FrameToDecode> decodable_frames_;
    std::unique_ptr<RepeatingTask> decode_repeating_task_ = nullptr;
    bool keyframe_required_ = false;
    int64_t waiting_deadline_ms_ = 0;
    NextFrameFoundCallback next_frame_found_callback_ = nullptr;
};
    
} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc

#endif