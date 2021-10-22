#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder_seq_num.hpp"

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {

std::unique_ptr<FrameRefFinder> FrameRefFinder::Create(VideoCodecType codec_type, int64_t picture_id_offset) {
    std::unique_ptr<FrameRefFinder> ref_finder = nullptr;

    switch (codec_type) {
    case VideoCodecType::H264:
        ref_finder = std::make_unique<SeqNumFrameRefFinder>(picture_id_offset);
        break;
    default:
        break;
    }

    return ref_finder;
}

FrameRefFinder::FrameRefFinder(int64_t picture_id_offset) 
    : picture_id_offset_(picture_id_offset),
      frame_ref_found_callback_(nullptr) {}

FrameRefFinder::~FrameRefFinder() = default;

void FrameRefFinder::InsertFrame(std::unique_ptr<video::FrameToDecode> frame) {
    static_assert("Implemented in derived class.");
}

void FrameRefFinder::InsertPadding(uint16_t seq_num) {
    static_assert("Implemented in derived class.");
}

void FrameRefFinder::ClearTo(uint16_t seq_num) {
    static_assert("Implemented in derived class.");
}

void FrameRefFinder::OnFrameRefFound(FrameRefFoundCallback callback) {
    frame_ref_found_callback_ = std::move(callback);
}

// Protected methods
void FrameRefFinder::SetPictureId(int64_t picture_id, video::FrameToDecode& frame) {
    frame.set_id(picture_id_offset_ + picture_id);
}

void FrameRefFinder::AddReference(int64_t picture_id, video::FrameToDecode& frame) {
    frame.AddReference(picture_id_offset_ + picture_id);
}

} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivertc
