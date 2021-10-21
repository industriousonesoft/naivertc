#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_REFERENCE_FINDER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_REFERENCE_FINDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp"

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {

// This class is not thread-saftey, the caller MUST provide that.
class RTC_CPP_EXPORT FrameRefFinder {
public:
    static std::unique_ptr<FrameRefFinder> Create(VideoCodecType codec_type);
public:
    virtual ~FrameRefFinder();

    virtual void InsertFrame(std::unique_ptr<video::FrameToDecode> frame) = 0;
    virtual void InsertPadding(uint16_t seq_num) = 0;
    virtual void ClearTo(uint16_t seq_num) = 0;

    using FrameRefFoundCallback = std::function<void(std::unique_ptr<video::FrameToDecode>)>;
    void OnFrameRefFound(FrameRefFoundCallback callback);

protected:
    FrameRefFinder();
protected:
    FrameRefFoundCallback frame_ref_found_callback_ = nullptr;
};
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivertc

#endif