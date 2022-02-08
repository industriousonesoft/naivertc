#ifndef _RTC_MEDIA_VIDEO_TRACK_H_
#define _RTC_MEDIA_VIDEO_TRACK_H_

#include "base/defines.hpp"
#include "base/thread_annotation.hpp"
#include "rtc/media/media_track.hpp"
#include "rtc/media/video/encoded_frame.hpp"


namespace naivertc {

class RTC_CPP_EXPORT VideoTrack : public MediaTrack {
public:
    using MediaTrack::MediaTrack;
    ~VideoTrack() override;

    void Send(video::EncodedFrame encoded_frame);
};
    
} // namespace naivertc

#endif