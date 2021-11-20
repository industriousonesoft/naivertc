#ifndef _RTC_MEDIA_VIDEO_VIDEO_RECEIVE_STREAM_H_
#define _RTC_MEDIA_VIDEO_VIDEO_RECEIVE_STREAM_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/call/rtp_video_stream_receiver.hpp"

#include <map>

namespace naivertc {

class RTC_CPP_EXPORT VideoReceiveStream {
public:
    struct Configuration {
        using Rtp = struct RtpVideoStreamReceiver::Configuration;
        Rtp rtp;
    };  
public:
    VideoReceiveStream(Configuration config, 
                       std::shared_ptr<TaskQueue> task_queue);
    ~VideoReceiveStream();

private:
    const Configuration config_;
    std::shared_ptr<TaskQueue> task_queue_;
};

} // namespace naivertc

#endif