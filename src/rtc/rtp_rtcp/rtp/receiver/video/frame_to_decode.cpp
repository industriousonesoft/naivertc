#include "rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp"

namespace naivertc {
namespace rtc {
namespace video {

FrameToDecode::FrameToDecode(VideoFrameType frame_type,
                             VideoCodecType codec_type, 
                             uint16_t first_packet_seq_num, 
                             uint16_t last_packet_seq_num) 
    : frame_type_(frame_type),
      codec_type_(codec_type),
      first_packet_seq_num_(first_packet_seq_num),
      last_packet_seq_num_(last_packet_seq_num) {}

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