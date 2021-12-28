#ifndef _RTC_CHANNELS_MEDIA_CHANNEL_H_
#define _RTC_CHANNELS_MEDIA_CHANNEL_H_

#include "base/defines.hpp"
#include "rtc/api/media_transport.hpp"
#include "rtc/base/time/clock_real_time.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "rtc/media/video_send_stream.hpp"
#include "rtc/media/video_receive_stream.hpp"

#include <iostream>

namespace naivertc {

// MediaChannel
class RTC_CPP_EXPORT MediaChannel {
public:
    virtual ~MediaChannel();

    const std::string mid() const;

    void Open(std::weak_ptr<MediaTransport> transport);
    void Close();

    virtual void SetLocalMedia(sdp::Media media, sdp::Type type);
    virtual void SetRemoteMedia(sdp::Media media, sdp::Type type);

protected:
    MediaChannel(std::string mid, TaskQueue* worker_queue);

protected:
    const std::string mid_;
    TaskQueue* worker_queue_;

    std::weak_ptr<MediaTransport> send_transport_;
};

} // nemespace naivertc

#endif