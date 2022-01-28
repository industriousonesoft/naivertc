#include "rtc/media/video_receive_stream.hpp"

namespace naivertc {

VideoReceiveStream::VideoReceiveStream(Configuration config) 
    : config_(std::move(config)) {}

VideoReceiveStream::~VideoReceiveStream() {
    RTC_RUN_ON(&sequence_checker_);
};

std::vector<uint32_t> VideoReceiveStream::ssrcs() const {
    RTC_RUN_ON(&sequence_checker_);
    return ssrcs_;
}

void VideoReceiveStream::OnRtpPacket(RtpPacketReceived in_packet) {}

void VideoReceiveStream::OnRtcpPacket(CopyOnWriteBuffer in_packet) {}

} // namespace naivertc