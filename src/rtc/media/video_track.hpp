#ifndef _RTC_MEDIA_VIDEO_TRACK_H_
#define _RTC_MEDIA_VIDEO_TRACK_H_

#include "base/defines.hpp"
#include "base/thread_annotation.hpp"
#include "rtc/media/media_track.hpp"

namespace naivertc {

class RTC_CPP_EXPORT VideoTrack : public MediaTrack {
public:
    using MediaTrack::MediaTrack;
    ~VideoTrack() override;

private:
    void Open(std::weak_ptr<MediaTransport> transport) override {};
    void Close() override {};
};
    
} // namespace naivertc

#endif