#include "rtc/call/video_receive_stream.hpp"

namespace naivertc {

VideoReceiveStream::VideoReceiveStream(Configuration config) 
    : config_(std::move(config)){}

VideoReceiveStream::~VideoReceiveStream() {};

} // namespace naivertc