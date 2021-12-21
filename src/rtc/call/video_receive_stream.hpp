#ifndef _RTC_CALL_VIDEO_VIDEO_RECEIVE_STREAM_H_
#define _RTC_CALL_VIDEO_VIDEO_RECEIVE_STREAM_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/call/rtp_video_receiver.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

#include <map>

namespace naivertc {

class RTC_CPP_EXPORT VideoReceiveStream {
public:
    struct Configuration {
        using Rtp = struct RtpVideoReceiver::Configuration;
        Rtp rtp;
    };  
public:
    VideoReceiveStream(Configuration config);
    ~VideoReceiveStream();

private:
    SequenceChecker sequence_checker_;
    const Configuration config_;
};

} // namespace naivertc

#endif