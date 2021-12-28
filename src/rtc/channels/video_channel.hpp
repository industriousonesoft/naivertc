#ifndef _RTC_CHANNELS_VIDEO_CHANNEL_H_
#define _RTC_CHANNELS_VIDEO_CHANNEL_H_

#include "base/defines.hpp"
#include "rtc/channels/media_channel.hpp"

namespace naivertc {

class RTC_CPP_EXPORT VideoChannel : public MediaChannel {
public:
    VideoChannel(std::string mid, TaskQueue* worker_queue);
    ~VideoChannel() override;

    void SetLocalMedia(sdp::Media media, sdp::Type type) override;
    void SetRemoteMedia(sdp::Media media, sdp::Type type) override;
};

} // namespace naivertc

#endif