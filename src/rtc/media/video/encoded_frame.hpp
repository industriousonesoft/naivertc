#ifndef _RTC_MEDIA_VIDEO_VIDEO_ENCODED_FRAME_H_
#define _RTC_MEDIA_VIDEO_VIDEO_ENCODED_FRAME_H_

#include "base/defines.hpp"
#include "rtc/media/video/common.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"

namespace naivertc {
namespace video {
    
class RTC_CPP_EXPORT EncodedFrame : public CopyOnWriteBuffer {
public:
    EncodedFrame();
    ~EncodedFrame();

    // frame timestamp (90kHz)
    uint32_t timestamp() const { return timestamp_; }
    void set_timestamp(uint32_t timestamp) { timestamp_ = timestamp; }

    int64_t capture_time_ms() const { return capture_time_ms_; }
    void set_capture_time_ms(int64_t time_ms) { capture_time_ms_ = time_ms; }

    bool retransmission_allowed() const { return retransmission_allowed_; }
    void set_retransmission_allowed(bool retransmission_allowed) { retransmission_allowed_ = retransmission_allowed; }

    video::FrameType frame_type() const { return frame_type_; }
    void set_frame_type(video::FrameType type) { frame_type_ = type; }

private:
    uint32_t timestamp_ = 0;
    int64_t capture_time_ms_ = 0;
    // Retransmission is allowed as default state
    bool retransmission_allowed_ = true;

    video::FrameType frame_type_ = video::FrameType::EMPTTY;
};
    
} // namespace video
} // namespace naivertc

#endif