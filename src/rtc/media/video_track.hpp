#ifndef _RTC_MEDIA_VIDEO_TRACK_H_
#define _RTC_MEDIA_VIDEO_TRACK_H_

#include "base/defines.hpp"
#include "rtc/media/media_track.hpp"
#include "rtc/media/video_send_stream.hpp"
#include "rtc/media/video_receive_stream.hpp"

namespace naivertc {

class RTC_CPP_EXPORT VideoTrack : public MediaTrack {
public:
    VideoTrack(const Configuration& config, TaskQueue* task_queue);
    VideoTrack(sdp::Media remote_description, TaskQueue* task_queue);
    ~VideoTrack() override;

    VideoSendStream* AddSendStream();
    VideoReceiveStream* AddRecvStream();

private:
    VideoSendStream::Configuration BuildSendConfig(const sdp::Media& description) const;

    TaskQueue* SendQueue();
    TaskQueue* RecvQueue();

private:
    std::unique_ptr<TaskQueue> send_queue_ = nullptr;
    std::unique_ptr<TaskQueue> recv_queue_ = nullptr;

    std::unique_ptr<VideoSendStream> send_stream_ = nullptr;
    std::unique_ptr<VideoReceiveStream> recv_stream_ = nullptr;

};
    
} // namespace naivertc

#endif