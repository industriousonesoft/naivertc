#ifndef _RTC_MEDIA_AUDIO_TRACK_H_
#define _RTC_MEDIA_AUDIO_TRACK_H_

#include "base/defines.hpp"
#include "rtc/media/media_track.hpp"

namespace naivertc {

class AudioTrack : public MediaTrack {
public:
    using MediaTrack::MediaTrack;
    ~AudioTrack() override;
};
    
} // namespace naivertc

#endif