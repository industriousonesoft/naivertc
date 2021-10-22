#include "rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp"

namespace naivertc {
namespace rtc {
namespace video {

FrameToDecode::FrameToDecode(VideoFrameType frame_type,
                             VideoCodecType codec_type, 
                             uint16_t seq_num_start, 
                             uint16_t seq_num_end,
                             uint32_t timestamp,
                             CopyOnWriteBuffer bitstream)
    : frame_type_(frame_type),
      codec_type_(codec_type),
      seq_num_start_(seq_num_start),
      seq_num_end_(seq_num_end),
      timestamp_(timestamp),
      bitstream_(std::move(bitstream)) {}

FrameToDecode::~FrameToDecode() {}

void FrameToDecode::ForEachReference(std::function<void(int64_t picture_id)> callback) const {
    if (!callback) return;
    for (int64_t picture_id : referred_picture_ids_) {
        callback(picture_id);
    }
}
    
} // namespace video
} // namespace rtc
} // namespace naivertc 