#ifndef _RTC_MEDIA_AUDIO_TRACK_H_
#define _RTC_MEDIA_AUDIO_TRACK_H_

#include "base/defines.hpp"
#include "rtc/media/media_track.hpp"

namespace naivertc {

class RTC_CPP_EXPORT AudioTrack : public MediaTrack {
public:
    using MediaTrack::MediaTrack;
    ~AudioTrack() override;

private:
    void Open(std::weak_ptr<MediaTransport> transport) override {};
    void Close() override {};
    void OnMediaNegotiated(const sdp::Media& local_media, 
                           const sdp::Media& remote_media, 
                           sdp::Type remote_type) override {}
};
    
} // namespace naivertc

#endif