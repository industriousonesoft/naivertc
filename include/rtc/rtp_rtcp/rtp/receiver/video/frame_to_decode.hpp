#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_TO_DECODE_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_FRAME_TO_DECODE_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/media/video/common.hpp"

namespace naivertc {
namespace rtc {
namespace video {

class RTC_CPP_EXPORT FrameToDecode {
public:
    FrameToDecode(VideoFrameType frame_type,
                  VideoCodecType codec_type, 
                  uint16_t seq_num_start, 
                  uint16_t seq_num_end);
    ~FrameToDecode();

    void set_id(int64_t id) { id_ = id; }
    int64_t id() const { return id_; }

    VideoFrameType frame_type() const { return frame_type_; }
    VideoCodecType codec_type() const { return codec_type_; }
    uint16_t seq_num_start() const { return seq_num_start_; }
    uint16_t seq_num_end() const { return seq_num_end_; }

    void AddReference(int64_t picture_id) { referred_picture_ids_.push_back(picture_id); }
    size_t NumOfReferences() const { return referred_picture_ids_.size(); }
    void ForEachReference(std::function<void(int64_t picture_id)>) const;

private:
    // Used to describe order and dependencies between frames.
    int64_t id_ = -1;

    VideoFrameType frame_type_;
    VideoCodecType codec_type_;
    uint16_t seq_num_start_;
    uint16_t seq_num_end_;

    std::vector<int64_t> referred_picture_ids_;
};
    
} // namespace video
} // namespace rtc
} // namespace naivertc


#endif