#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_BUFFER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_BUFFER_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp"

#include <optional>

#include <map>

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {

// The class is not thread-saftey, the caller MUST privode that.
class RTC_CPP_EXPORT FrameBuffer {
public:
    enum class ReturnReason { FOUND, TIME_OUT, STOPPED };
public:
    FrameBuffer();
    ~FrameBuffer();

    int64_t InsertFrame(std::unique_ptr<video::FrameToDecode> frame);

    void Clear();

private:
    bool ValidReferences(const video::FrameToDecode& frame);

private:
    struct FrameInfo {
        FrameInfo();
        FrameInfo(FrameInfo&&);
        ~FrameInfo();

        bool continuous = false;
        std::unique_ptr<video::FrameToDecode> frame = nullptr;
    };

    using FrameMap = std::map<int64_t, FrameInfo>;

private:
    std::shared_ptr<Clock> clock_;

    std::optional<int64_t> last_continuous_frame_id_ = std::nullopt;

    FrameMap frame_buffer_;
    std::vector<FrameMap::iterator> frame_to_decode_;
};
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivertc

#endif