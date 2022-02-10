#include "rtc/media/video_receive_stream.hpp"

namespace naivertc {

VideoReceiveStream::VideoReceiveStream(Configuration config) 
    : clock_(config.clock),
      decode_queue_(std::make_unique<TaskQueue>("VideoDecodeQueue")),
      rtp_receive_stats_(std::make_unique<RtpReceiveStatistics>(clock_)),
      timing_(std::make_unique<rtp::video::Timing>(clock_)),
      frame_buffer_(std::make_unique<rtp::video::jitter::FrameBuffer>(clock_, timing_.get(), decode_queue_.get(), nullptr)),
      rtp_video_receiver_(config.rtp, clock_, rtp_receive_stats_.get(), this) {

    rtp_demuxer_.AddRtpSink(config.rtp.local_ssrc, &rtp_video_receiver_);

    // RTX stream
    if (config.rtp.rtx_ssrc > 0) {
        rtx_recv_stream_ = std::make_unique<RtxReceiveStream>(config.rtp.local_ssrc, config.rtp.rtx_associated_payload_types);
        rtp_demuxer_.AddRtpSink(config.rtp.rtx_ssrc, rtx_recv_stream_.get());
    }
}

VideoReceiveStream::~VideoReceiveStream() {
    RTC_RUN_ON(&sequence_checker_);
};

std::vector<uint32_t> VideoReceiveStream::ssrcs() const {
    RTC_RUN_ON(&sequence_checker_);
    return ssrcs_;
}

void VideoReceiveStream::OnRtpPacket(RtpPacketReceived in_packet) {
   RTC_RUN_ON(&sequence_checker_);
   rtp_demuxer_.OnRtpPacket(std::move(in_packet), /*is_rtcp=*/false);
}

void VideoReceiveStream::OnRtcpPacket(CopyOnWriteBuffer in_packet) {
    RTC_RUN_ON(&sequence_checker_);
    rtp_video_receiver_.OnRtcpPacket(std::move(in_packet));
}

void VideoReceiveStream::OnCompleteFrame(rtp::video::FrameToDecode frame) {
    RTC_RUN_ON(&sequence_checker_);
}

} // namespace naivertc