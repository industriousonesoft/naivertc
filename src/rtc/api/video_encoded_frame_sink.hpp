#ifndef _RTC_API_VIDEO_ENCODED_FRAME_SINK_H_
#define _RTC_API_VIDEO_ENCODED_FRAME_SINK_H_

#include "base/defines.hpp"
#include "rtc/media/video/encoded_frame.hpp"

namespace naivertc {

class RTC_CPP_EXPORT VideoEncodedFrameSink {
public:
    virtual bool OnEncodedFrame(video::EncodedFrame encoded_frame) = 0;
protected:
    virtual ~VideoEncodedFrameSink() = default;
};
    
} // namespace naivertc


#endif