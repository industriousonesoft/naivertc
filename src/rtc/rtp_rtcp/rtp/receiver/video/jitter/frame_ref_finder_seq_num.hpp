#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_REF_FINDER_SEQ_NUM_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_REF_FINDER_SEQ_NUM_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"
#include "rtc/rtp_rtcp/components/num_unwrapper.hpp"

#include <map>
#include <set>
#include <deque>
#include <functional>

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {

// This class is not thread-safe, the caller MUST provide that.
class SeqNumFrameRefFinder : public FrameRefFinder {
public:
    SeqNumFrameRefFinder(int64_t picture_id_offset);
    ~SeqNumFrameRefFinder() override;

    void InsertFrame(video::FrameToDecode frame) override;
    void InsertPadding(uint16_t seq_num) override;
    void ClearTo(uint16_t seq_num) override;

private:
    enum class FrameDecision { STASHED, HAND_OFF, DROPED };

    FrameDecision FindRefForFrame(video::FrameToDecode& frame);
    void UpdateGopInfo(uint16_t seq_num);
    void RetryStashedFrames();

private:
    using PictureId = uint16_t;
    struct GopInfo {
        PictureId last_picture_id_gop;
        PictureId last_picture_id_with_padding_gop;
    };
    // Using the picture id of the keyframe in the GOP as the key.
    std::map<PictureId, GopInfo, wrap_around_utils::OlderThan<PictureId>> gop_infos_;

    std::set<PictureId, wrap_around_utils::OlderThan<PictureId>> stashed_padding_;
    std::deque<video::FrameToDecode> stashed_frames_;

    SeqNumUnwrapper seq_num_unwrapper_;
};
    
} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc

#endif