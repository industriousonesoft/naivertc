#include "rtc/call/call.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/transports/rtc_transport_media.hpp"
#include "rtc/media/video_send_stream.hpp"
#include "rtc/media/video_receive_stream.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"
#include "rtc/call/rtp_send_controller.hpp"

namespace naivertc {
namespace {

bool UseSendSideBwe(const std::vector<RtpExtension>& extensions) {
    for (const auto& extension : extensions) {
        if (extension.uri == RtpExtension::kTransportSequenceNumberUri ||
            extension.uri == RtpExtension::kTransportSequenceNumberV2Uri) {
            return true;
        }
    }
    return false;
}

bool SendPeriodicFeedback(const std::vector<RtpExtension>& extensions) {
    for (const auto& extension : extensions) {
        // NOTE: Indicates the receiver will not send transport feedback periodically,
        // but responding to the feedback request sent by send side.
        if (extension.uri == RtpExtension::kTransportSequenceNumberV2Uri) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<RtpSendController> CreateSendController(Clock* clock) {
    RtpSendController::Configuration config;
    config.clock = clock;
    // TODO: Initial target bitrate settings.
    return std::make_unique<RtpSendController>(config);
}

} // namespace

Call::Call(Clock* clock, RtcMediaTransport* send_transport) 
    : clock_(clock),
      send_transport_(send_transport),
      send_controller_(CreateSendController(clock_)) {
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
        auto it = recv_streams_by_ssrc_.find(received_packet.ssrc());
        if (it == recv_streams_by_ssrc_.end()) {
            PLOG_WARNING << "Failed to look up RTP header extension for ssrc=" << received_packet.ssrc();
            return;
        }
        // Identify header extensions.
        auto header_extension = rtp::HeaderExtensionMap(it->second->rtp_params()->extensions);
        header_extension.set_extmap_allow_mixed(it->second->rtp_params()->extmap_allow_mixed);
        received_packet.SetHeaderExtensionMap(std::move(header_extension));

        // Deliver RTP packet.
        if (!rtp_demuxer_.DeliverRtpPacket(std::move(received_packet))) {
            PLOG_WARNING << "No sink found for packet with ssrc=" << received_packet.ssrc();
        }
    }
}

void Call::AddVideoSendStream(const RtpParameters& rtp_params) {
    RTC_RUN_ON(&worker_queue_checker_);
    if (!UseSendSideBwe(rtp_params.extensions)) {
        PLOG_WARNING << "The transport sequence number extension is required to enable send-side bandwidth estimation.";
        return;
    }
    
    if (rtp_params.local_media_ssrc > 0) {
        // Add video send stream.
        VideoSendStream::Configuration send_config;
        send_config.clock = clock_;
        send_config.send_transport = send_transport_;
        send_config.rtp = rtp_params;
        send_config.observers.bandwidth_observer = send_controller_.get();
        send_config.observers.rtcp_transport_feedback_observer = send_controller_.get();
        send_config.observers.rtp_transport_feedback_observer = send_controller_.get();
        auto send_stream = std::make_unique<VideoSendStream>(send_config);
        // Added as RTCP sink.
        for (uint32_t ssrc : send_stream->ssrcs()) {
            rtp_demuxer_.AddRtcpSink(ssrc, send_stream.get());
        }
        video_send_streams_.insert(std::move(send_stream));
    }

    OnAggregateNetworkStateChanged();
}

void Call::AddVideoRecvStream(const RtpParameters& rtp_params) {
    RTC_RUN_ON(&worker_queue_checker_);
    if (!UseSendSideBwe(rtp_params.extensions)) {
        PLOG_WARNING << "The transport sequence number extension is required to enable send-side bandwidth estimation.";
        return;
    }

    if (rtp_params.local_media_ssrc > 0) {
        VideoReceiveStream::Configuration recv_config;
        recv_config.clock = clock_;
        recv_config.send_transport = send_transport_;
        recv_config.rtp = rtp_params;
        auto recv_stream = std::make_unique<VideoReceiveStream>(recv_config);
        
        for (uint32_t ssrc : recv_stream->ssrcs()) {
            // Added as RTP sink.
            rtp_demuxer_.AddRtcpSink(ssrc, recv_stream.get());
            // Added as RTCP sink.
            rtp_demuxer_.AddRtcpSink(ssrc, recv_stream.get());
            // Rtp header extenson map.
            recv_streams_by_ssrc_.emplace(ssrc, recv_stream.get());
        }
        video_recv_streams_.insert(std::move(recv_stream));
    }

    OnAggregateNetworkStateChanged();
}

void Call::Clear() {
    RTC_RUN_ON(&worker_queue_checker_);
    rtp_demuxer_.Clear();
    send_controller_->Clear();
    video_send_streams_.clear();
    video_recv_streams_.clear();
    recv_streams_by_ssrc_.clear();
}

void Call::Send(video::EncodedFrame encoded_frame) {
    RTC_RUN_ON(&worker_queue_checker_);
    if (video_send_streams_.empty()) {
        return;
    }
    for (auto& send_stream: video_send_streams_) {
        send_stream->OnEncodedFrame(std::move(encoded_frame));
    }
}

// Private methods
void Call::OnAggregateNetworkStateChanged() {
    RTC_RUN_ON(&worker_queue_checker_);
    EnsureStarted();

    bool have_video = !video_send_streams_.empty() || 
                      !video_recv_streams_.empty();

    send_controller_->OnNetworkAvailability(have_video);
    
}

void Call::EnsureStarted() {
    RTC_RUN_ON(&worker_queue_checker_);
    if (is_started_) {
        return;
    }
    is_started_ = true;

    send_controller_->OnTargetTransferBitrateUpdated([this](TargetTransferBitrate target_bitrate){
        
    });

    send_controller_->EnsureStarted();
}
    
} // namespace naivertc
