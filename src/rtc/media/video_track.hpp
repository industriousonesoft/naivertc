#ifndef _RTC_MEDIA_VIDEO_TRACK_H_
#define _RTC_MEDIA_VIDEO_TRACK_H_

#include "base/defines.hpp"
#include "base/thread_annotation.hpp"
#include "rtc/media/media_track.hpp"
#include "rtc/media/video_send_stream.hpp"
#include "rtc/media/video_receive_stream.hpp"

namespace naivertc {

class RTC_CPP_EXPORT VideoTrack : public MediaTrack {
public:
    using MediaTrack::MediaTrack;
    ~VideoTrack() override;

private:
    void TriggerOpen() override {};
    void TriggerClose() override {};
private:
    VideoSendStream::Configuration BuildSendConfig(const sdp::Media& description) const;
};
    
} // namespace naivertc

#endif