#include "rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp"

namespace naivertc {
namespace rtp {
namespace video {

FrameToDecode::FrameToDecode(CopyOnWriteBuffer bitstream,
                             video::FrameType frame_type,
                             video::CodecType codec_type, 
                             uint16_t seq_num_start, 
                             uint16_t seq_num_end,
                             uint32_t timestamp,
                             int64_t ntp_time_ms,
                             int times_nacked,
                             int64_t min_received_time_ms,
                             int64_t max_received_time_ms)
    : CopyOnWriteBuffer(std::move(bitstream)),
      frame_type_(frame_type),
      codec_type_(codec_type),
      seq_num_start_(seq_num_start),
      seq_num_end_(seq_num_end),
      timestamp_(timestamp),
      ntp_time_ms_(ntp_time_ms),
      times_nacked_(times_nacked),
      min_received_time_ms_(min_received_time_ms),
      max_received_time_ms_(max_received_time_ms),
      render_time_ms_(-1) {}

FrameToDecode::~FrameToDecode() {}

void FrameToDecode::ForEachReference(std::function<void(int64_t frame_id, bool* stoped)> callback) const {
    if (!callback) return;
    bool stoped = false;
    for (int64_t frame_id : referred_frame_ids_) {
        callback(frame_id, &stoped);
        if (stoped) {
            break;
        }
    }
}
    
} // namespace video
} // namespace rtp
} // namespace naivertc 