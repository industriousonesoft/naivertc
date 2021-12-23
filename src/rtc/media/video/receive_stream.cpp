#include "rtc/media/video/receive_stream.hpp"

namespace naivertc {

VideoReceiveStream::VideoReceiveStream(Configuration config) 
    : config_(std::move(config)){}

VideoReceiveStream::~VideoReceiveStream() {};

void VideoReceiveStream::OnRtpPacket(RtpPacketReceived in_packet) {

}

void VideoReceiveStream::OnRtcpPacket(CopyOnWriteBuffer in_packet) {
    
}

} // namespace naivertc