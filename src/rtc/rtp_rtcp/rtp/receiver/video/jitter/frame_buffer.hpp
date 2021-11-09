#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_BUFFER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_BUFFER_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "common/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/inter_frame_delay.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/decoded_frames_history.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_defines.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_estimator.hpp"

#include <optional>
#include <map>
#include <vector>

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {

// The class is not thread-safe, the caller MUST privode that.
class RTC_CPP_EXPORT FrameBuffer {
public:
    enum class ReturnReason { FOUND, TIME_OUT, STOPPED };
public:
    FrameBuffer(std::shared_ptr<Clock> clock, 
                std::shared_ptr<Timing> timing,
                std::shared_ptr<TaskQueue> task_queue);
    ~FrameBuffer();

    ProtectionMode protection_mode() const;
    void set_protection_mode(ProtectionMode mode);

    bool keyframe_required() const;
    void set_keyframe_required(bool keyframe_required);

    void UpdateRtt(int64_t rtt_ms);

    int64_t InsertFrame(std::unique_ptr<video::FrameToDecode> frame);

    using NextFrameCallback = std::function<void(std::unique_ptr<video::FrameToDecode>, ReturnReason)>;
    void NextFrame(int64_t max_wait_time_ms, 
                   bool keyframe_required, 
                   std::shared_ptr<TaskQueue> task_queue, 
                   NextFrameCallback callback);

    void Clear();

private:
    struct FrameInfo {
        FrameInfo();
        FrameInfo(FrameInfo&&);
        ~FrameInfo();

        int64_t frame_id() const { return frame != nullptr ? frame->id() : -1; }

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

        std::unique_ptr<video::FrameToDecode> frame = nullptr;
    };
    using FrameMap = std::map<int64_t, FrameInfo>;

private:
    size_t NumUndecodedFrames(FrameMap::iterator begin, FrameMap::iterator end);

    // Continuity
    bool ValidReferences(const video::FrameToDecode& frame);
    bool UpdateFrameReferenceInfo(FrameInfo& frame_info);
    void PropagateContinuity(const FrameInfo& frame_info);

    // Decodability
    FrameMap::iterator FindNextDecodableFrame();
    int64_t FindNextFrame(int64_t now_ms);
    video::FrameToDecode* GetNextFrame();
    bool ValidRenderTiming(const video::FrameToDecode& frame, int64_t now_ms);
    void PropagateDecodability(const FrameInfo& frame_info);

private:
    static constexpr int64_t kLogNonDecodedIntervalMs = 5000;
private:
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<Timing> timing_;
    std::shared_ptr<TaskQueue> task_queue_;
    InterFrameDelay inter_frame_delay_;
    DecodedFramesHistory decoded_frames_history_;
    JitterEstimator jitter_estimator_;
    bool keyframe_required_;
    const bool add_rtt_to_playout_delay_;
    int64_t latest_render_time_ms_;
    int64_t last_log_non_decoded_ms_;
    ProtectionMode protection_mode_;

    std::optional<int64_t> last_continuous_frame_id_ = std::nullopt;

    FrameMap frames_;
    std::vector<FrameMap::iterator> frames_to_decode_;

};
    
} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc

#endif