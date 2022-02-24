#include "rtc/call/call.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/transports/rtc_transport_media.hpp"
#include "rtc/media/video_send_stream.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

namespace naivertc {

Call::Call(Clock* clock, RtcMediaTransport* send_transport) 
    : clock_(clock),
      send_transport_(send_transport),
      send_controller_(clock_) {
    worker_queue_checker_.Detach();
}
    
Call::~Call() {};
    
void Call::DeliverRtpPacket(CopyOnWriteBuffer in_packet, bool is_rtcp) {
    RTC_RUN_ON(&worker_queue_checker_);
    if (is_rtcp) {
        rtp_demuxer_.DeliverRtcpPacket(std::move(in_packet));
    } else {
        RtpPacketReceived received_packet;
        if (received_packet.Parse(std::move(in_packet))) {
            PLOG_WARNING << "Failed to parse the incoming RTP packet before demuxing. Drop it.";
            return;
        }
        rtp_demuxer_.DeliverRtpPacket(std::move(received_packet));
        auto it = recv_rtp_ext_maps_.find(received_packet.ssrc());
        if (it == recv_rtp_ext_maps_.end()) {
            PLOG_WARNING << "Failed to look up RTP header extension for ssrc=" << received_packet.ssrc();
            return;
        }
        // Identify header extensions.
        received_packet.SetHeaderExtensionMap(it->second);
    }
}

void Call::AddVideoSendStream(RtpParameters rtp_params) {
    RTC_RUN_ON(&worker_queue_checker_);
    if (rtp_params.local_media_ssrc > 0) {
        // Add video send stream.
        VideoSendStream::Configuration send_config;
        send_config.clock = clock_;
        send_config.send_transport = send_transport_;
        send_config.rtp = std::move(rtp_params);
        send_config.observers.bandwidth_observer = &send_controller_;
        send_config.observers.rtcp_transport_feedback_observer = &send_controller_;
        send_config.observers.rtp_transport_feedback_observer = &send_controller_;
        auto send_stream = std::make_unique<VideoSendStream>(send_config);
        // Added as RTCP sink.
        for (uint32_t ssrc : send_stream->ssrcs()) {
            rtp_demuxer_.AddRtcpSink(ssrc, send_stream.get());
        }
        video_send_streams_.insert(std::move(send_stream));
    }
}

void Call::AddVideoRecvStream(RtpParameters rtp_params) {
    RTC_RUN_ON(&worker_queue_checker_);

    // auto header_extension = rtp::HeaderExtensionMap(rtp_params.extensions)
    // header_extension.set_extmap_allow_mixed(rtp_params.extmap_allow_mixed);
}

void Call::Clear() {
    RTC_RUN_ON(&worker_queue_checker_);
    rtp_demuxer_.Clear();
}

void Call::Send(video::EncodedFrame encoded_frame) {
    if (video_send_streams_.empty()) {
        return;
    }
    for (auto& send_stream: video_send_streams_) {
        send_stream->OnEncodedFrame(std::move(encoded_frame));
    }
}
    
} // namespace naivertc