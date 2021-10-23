#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_BUFFER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_BUFFER_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/decoded_frames_history.hpp"

#include <optional>
#include <map>
#include <vector>

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {

// The class is not thread-saftey, the caller MUST privode that.
class RTC_CPP_EXPORT FrameBuffer {
public:
    enum class ReturnReason { FOUND, TIME_OUT, STOPPED };
public:
    FrameBuffer(std::shared_ptr<Clock> clock);
    ~FrameBuffer();

    int64_t InsertFrame(std::unique_ptr<video::FrameToDecode> frame);

    void Clear();

private:
    struct FrameInfo {
        FrameInfo();
        FrameInfo(FrameInfo&&);
        ~FrameInfo();

        int64_t frame_id() const { return frame != nullptr ? frame->id() : -1; }

        // A frame is continuous if it has all its referenced/indirectly referenced frames.
        // Indicate how many unfulfilled frames this frame have until it becomes continuous.
        size_t num_missing_continuous = 0;

        // A frame is decodable if it has all its referenced frames have been decoded.
        // Indicate how many unfulfilled frames this frame have until it becomes decodable.
        size_t num_missing_decodable = 0;

        // Which other frames that have direct unfulfilled dependencies on this frame.
        std::vector<int64_t> dependent_frames;

        // Indicate if the frame is continuous or not.
        bool continuous = false;
        std::unique_ptr<video::FrameToDecode> frame = nullptr;
    };
    using FrameMap = std::map<int64_t, FrameInfo>;

private:
    bool ValidReferences(const video::FrameToDecode& frame);
    bool UpdateFrameInfo(FrameInfo& frame_info);

    void PropagateContinuity(const FrameInfo& frame_info);
private:
    std::shared_ptr<Clock> clock_;

    std::optional<int64_t> last_continuous_frame_id_ = std::nullopt;

    FrameMap frame_buffer_;
    std::vector<FrameMap::iterator> frame_to_decode_;

    DecodedFramesHistory decoded_frames_history_;

    int64_t last_log_non_decoded_ms_;
};
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivertc

#endif