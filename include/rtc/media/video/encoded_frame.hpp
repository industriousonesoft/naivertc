#ifndef _RTC_MEDIA_VIDEO_VIDEO_ENCODED_FRAME_H_
#define _RTC_MEDIA_VIDEO_VIDEO_ENCODED_FRAME_H_

#include "base/defines.hpp"

namespace naivertc {

class RTC_CPP_EXPORT VideoEncodedFrame : public BinaryBuffer {
public:
    VideoEncodedFrame();
    ~VideoEncodedFrame();

    // 90kHz
    uint32_t timestamp() const { return timestamp_; }
    void set_timestamp(uint32_t timestamp) { timestamp_ = timestamp; }

    bool retransmission_allowed() const { return retransmission_allowed_; }
    void set_retransmission_allowed(bool retransmission_allowed) { retransmission_allowed_ = retransmission_allowed; }

private:
    uint32_t timestamp_ = 0;
    // Retransmission is allowed as default state
    bool retransmission_allowed_ = true;
};
    
} // namespace naivertc


#endif