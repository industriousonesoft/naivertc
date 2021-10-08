#ifndef _RTC_MEDIA_VIDEO_VIDEO_RECEIVE_STREAM_H_
#define _RTC_MEDIA_VIDEO_VIDEO_RECEIVE_STREAM_H_

#include "base/defines.hpp"
#include "common/task_queue.hpp"

#include <map>

namespace naivertc {

class RTC_CPP_EXPORT VideoReceiveStream {
public:
    struct Configuration {
        struct Rtp {
            // Sender SSRC used for sending RTCP (such as receiver reports).
            uint32_t local_ssrc = 0;
            // Synchronization source to be received.
            uint32_t remote_ssrc = 0;

            int ulpfec_payload_type = -1;
            int red_payload_type = -1;

            uint32_t rtx_ssrc = 0;
            std::map<int, int> rtx_associated_payload_types;

            // Set if the stream is protected using FlexFEC.
            bool protected_by_flexfec = false;
        };
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