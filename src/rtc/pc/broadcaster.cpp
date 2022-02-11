#include "rtc/pc/broadcaster.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/api/media_transport.hpp"
#include "rtc/media/video_send_stream.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

namespace naivertc {

Broadcaster::Broadcaster(Clock* clock, MediaTransport* send_transport) 
    : clock_(clock),
      send_transport_(send_transport) {
    worker_queue_checker_.Detach();
}
    
Broadcaster::~Broadcaster() {};
    
void Broadcaster::DeliverRtpPacket(CopyOnWriteBuffer in_packet, bool is_rtcp) {
    RTC_RUN_ON(&worker_queue_checker_);
    if (is_rtcp) {
        rtp_demuxer_.DeliverRtcpPacket(std::move(in_packet));
    } else {
        // TODO: Using RTP header extension map and arrival time as initial parameters
        RtpPacketReceived received_packet;
        if (received_packet.Parse(std::move(in_packet))) {
            PLOG_WARNING << "Failed to parse the incoming RTP packet before demuxing. Drop it.";
            return;
        }
        rtp_demuxer_.DeliverRtpPacket(std::move(received_packet));
    }
}

void Broadcaster::AddVideoSendStream(RtpParameters rtp_params) {
    RTC_RUN_ON(&worker_queue_checker_);
    if (rtp_params.local_media_ssrc > 0) {
        // Add video send stream.
        VideoSendStream::Configuration send_config;
        send_config.clock = clock_;
        send_config.send_transport = send_transport_;
        send_config.rtp = std::move(rtp_params);
        auto send_stream = std::make_unique<VideoSendStream>(std::move(send_config));
        // Added as RTCP sink.
        for (uint32_t ssrc : send_stream->ssrcs()) {
            rtp_demuxer_.AddRtcpSink(ssrc, send_stream.get());
        }
        video_send_streams_[rtp_params.local_media_ssrc] = std::move(send_stream);
    }
}

void Broadcaster::AddVideoRecvStream(RtpParameters rtp_params) {
    RTC_RUN_ON(&worker_queue_checker_);
}

void Broadcaster::Clear() {
    RTC_RUN_ON(&worker_queue_checker_);
    rtp_demuxer_.Clear();
}

void Broadcaster::Send(video::EncodedFrame encoded_frame) {
    if (video_send_streams_.empty()) {
        return;
    }
    for (auto& [ssrc, send_stream] : video_send_streams_) {
        send_stream->OnEncodedFrame(std::move(encoded_frame));
    }
}
    
} // namespace naivertc
