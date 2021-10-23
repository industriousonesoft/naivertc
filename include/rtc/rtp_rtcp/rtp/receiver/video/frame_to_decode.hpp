#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_TO_DECODE_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_TO_DECODE_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/media/video/common.hpp"

#include <set>

namespace naivertc {
namespace rtc {
namespace video {

class RTC_CPP_EXPORT FrameToDecode {
public:
    FrameToDecode(VideoFrameType frame_type,
                  VideoCodecType codec_type, 
                  uint16_t seq_num_start, 
                  uint16_t seq_num_end,
                  uint32_t timestamp,
                  int times_nacked,
                  int64_t min_received_time_ms,
                  int64_t max_received_time_ms,
                  CopyOnWriteBuffer bitstream);
    ~FrameToDecode();

    void set_id(int64_t id) { id_ = id; }
    int64_t id() const { return id_; }

    VideoFrameType frame_type() const { return frame_type_; }
    VideoCodecType codec_type() const { return codec_type_; }
    uint16_t seq_num_start() const { return seq_num_start_; }
    uint16_t seq_num_end() const { return seq_num_end_; }
    uint32_t timestamp() const { return timestamp_; }
    int times_nacked() const { return times_nacked_; }
    bool delayed_by_retransmission() const { return times_nacked_ > 0; }
    int64_t received_time_ms() const { return max_received_time_ms_; }

    int64_t render_time_ms() const { return render_time_ms_; }
    void set_render_time_ms(int64_t time_ms) { render_time_ms_ = time_ms; }

    bool is_keyframe() const { return frame_type_ == VideoFrameType::KEY && referred_picture_ids_.size() == 0; }

    bool InsertReference(int64_t picture_id) { return referred_picture_ids_.insert(picture_id).second; }
    size_t NumReferences() const { return referred_picture_ids_.size(); }
    void ForEachReference(std::function<void(int64_t picture_id, bool* stoped)>) const;

private:
    // Used to describe order and dependencies between frames.
    int64_t id_ = -1;

    VideoFrameType frame_type_;
    VideoCodecType codec_type_;
    uint16_t seq_num_start_;
    uint16_t seq_num_end_;
    uint32_t timestamp_;
    int times_nacked_;
    int64_t min_received_time_ms_;
    int64_t max_received_time_ms_;
    int64_t render_time_ms_;

    CopyOnWriteBuffer bitstream_;

    std::set<int64_t> referred_picture_ids_;
};
    
} // namespace video
} // namespace rtc
} // namespace naivertc


#endif