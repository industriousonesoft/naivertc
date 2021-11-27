#include "rtc/call/video_receive_stream.hpp"

namespace naivertc {

VideoReceiveStream::VideoReceiveStream(Configuration config, 
                                       std::shared_ptr<TaskQueue> task_queue) 
    : config_(std::move(config)),
      task_queue_(std::move(task_queue)) {}

VideoReceiveStream::~VideoReceiveStream() = default;

} // namespace naivertc