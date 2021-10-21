#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder_seq_num.hpp"

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {

std::unique_ptr<FrameRefFinder> FrameRefFinder::Create(VideoCodecType codec_type) {
    std::unique_ptr<FrameRefFinder> ref_finder = nullptr;

    switch (codec_type) {
    case VideoCodecType::H264:
        ref_finder = std::make_unique<SeqNumFrameRefFinder>();
        break;
    default:
        break;
    }

    return ref_finder;
}

FrameRefFinder::~FrameRefFinder() = default;

void FrameRefFinder::OnFrameRefFound(FrameRefFoundCallback callback) {
    frame_ref_found_callback_ = std::move(callback);
}

} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivertc
