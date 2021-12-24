#ifndef _RTC_MEDIA_AUDIO_TRACK_H_
#define _RTC_MEDIA_AUDIO_TRACK_H_

#include "base/defines.hpp"
#include "rtc/media/media_track.hpp"

namespace naivertc {

class RTC_CPP_EXPORT AudioTrack : public MediaTrack {
public:
    AudioTrack(const Configuration& config, TaskQueue* task_queue);
    AudioTrack(sdp::Media remote_description, TaskQueue* task_queue);
    ~AudioTrack() override;
};
    
} // namespace naivertc

#endif