#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_REFERENCE_FINDER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_REFERENCE_FINDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp"

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {

// This class is not thread-safe, the caller MUST provide that.
class RTC_CPP_EXPORT FrameRefFinder {
public:
    static std::unique_ptr<FrameRefFinder> Create(VideoCodecType codec_type, int64_t picture_id_offset = 0);
public:
    virtual ~FrameRefFinder();

    virtual void InsertFrame(std::unique_ptr<video::FrameToDecode> frame);
    virtual void InsertPadding(uint16_t seq_num);
    virtual void ClearTo(uint16_t seq_num);

    using FrameRefFoundCallback = std::function<void(std::unique_ptr<video::FrameToDecode>)>;
    void OnFrameRefFound(FrameRefFoundCallback callback);

protected:
    FrameRefFinder(int64_t picture_id_offset);

    void SetPictureId(int64_t picture_id, video::FrameToDecode& frame);
    bool InsertReference(int64_t picture_id, video::FrameToDecode& frame);
protected:
    const int64_t picture_id_offset_;
    FrameRefFoundCallback frame_ref_found_callback_ = nullptr;   
};
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivertc

#endif