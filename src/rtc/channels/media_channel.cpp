#include "rtc/channels/media_channel.hpp"
#include "rtc/media/video_send_stream.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

RtpParameters ParseRtpParameters(const sdp::Media& media) {
    RtpParameters rtp_parameters;
    // Ssrcs
    // Media ssrc
    if (!media.media_ssrcs().empty()) {
        rtp_parameters.local_media_ssrc = media.media_ssrcs()[0];
    }
    // Rtx ssrc
    if (!media.rtx_ssrcs().empty()) {
        rtp_parameters.rtx_send_ssrc = media.rtx_ssrcs()[0];
    }
    // FlexFec ssrc
    if (!media.fec_ssrcs().empty()) {
        rtp_parameters.flexfec.ssrc = media.fec_ssrcs()[0];
    }
    
    // Payload types
    media.ForEachRtpMap([&](const sdp::Media::RtpMap& rtp_map){
        switch (rtp_map.codec)
        {
        case sdp::Media::Codec::H264: {
            rtp_parameters.media_payload_type = rtp_map.payload_type;
            rtp_parameters.media_rtx_payload_type = rtp_map.rtx_payload_type;
            for (const auto& rtcp_feedback : rtp_map.rtcp_feedbacks) {
                // NACK enabled
                if (rtcp_feedback == sdp::Media::RtcpFeedback::NACK) {
                    rtp_parameters.nack_enabled = true;
                }
            }
            break;
        }
        case sdp::Media::Codec::RED: {
            rtp_parameters.ulpfec.red_payload_type = rtp_map.payload_type;
            rtp_parameters.ulpfec.red_rtx_payload_type = rtp_map.rtx_payload_type;
            break;
        }
        case sdp::Media::Codec::ULP_FEC: {
            rtp_parameters.ulpfec.ulpfec_payload_type = rtp_map.payload_type;
            break;
        }
        default:
            // TODO: Support more codecs.
            break;
        }
    });

    return rtp_parameters;
}

    
} // namespace


MediaChannel::MediaChannel(Kind kind, std::string mid, TaskQueue* worker_queue)
    : kind_(kind),
      mid_(std::move(mid)),
      clock_(std::make_unique<RealTimeClock>()),
      signaling_queue_(TaskQueueImpl::Current()),
      worker_queue_(worker_queue) {
    assert(signaling_queue_ != nullptr);
    assert(worker_queue_ != nullptr);
}

MediaChannel::~MediaChannel() {}

MediaChannel::Kind MediaChannel::kind() const {
    return kind_;
}

const std::string MediaChannel::mid() const {
    return mid_;
}

bool MediaChannel::is_opened() const {
    RTC_RUN_ON(signaling_queue_);
    return is_opened_;
}

void MediaChannel::OnOpened(OpenedCallback callback) {
    signaling_queue_->Post([this, callback=std::move(callback)](){
        opened_callback_ = callback;
        if (is_opened_ && opened_callback_) {
            opened_callback_();
        }
    });
}

void MediaChannel::OnClosed(ClosedCallback callback) {
    signaling_queue_->Post([this, callback=std::move(callback)](){
        closed_callback_ = callback;
    });
}

void MediaChannel::Open(std::weak_ptr<MediaTransport> transport) {
    RTC_RUN_ON(signaling_queue_);
    send_transport_ = std::move(transport);
    TriggerOpen();
    worker_queue_->Async([this](){
        CreateStreams();
    });
}

void MediaChannel::Close() {
    RTC_RUN_ON(signaling_queue_);
    TriggerClose();
    worker_queue_->Async([this](){
        send_rtp_parameters_.reset();
        send_stream_.reset();
        signaling_queue_->Post([this](){
            send_transport_.reset();
        });
    });
}

void MediaChannel::OnMediaNegotiated(sdp::Media local_media, 
                                     sdp::Media remote_media, 
                                     sdp::Type remote_sdp_type) {
    RTC_RUN_ON(worker_queue_);
    if (kind_ == Kind::VIDEO) {
        // Sendable
        if (local_media.direction() == sdp::Direction::SEND_ONLY ||
            local_media.direction() == sdp::Direction::SEND_RECV) {
            auto rtp_params = ParseRtpParameters(local_media);
            // Media ssrc
            if (rtp_params.local_media_ssrc >= 0) {
                send_ssrcs_.push_back(rtp_params.local_media_ssrc);
            }
            // RTX ssrc
            if (rtp_params.rtx_send_ssrc) {
                send_ssrcs_.push_back(*rtp_params.rtx_send_ssrc);
            }
            // FLEX_FEC ssrc
            if (rtp_params.flexfec.payload_type >= 0) {
                send_ssrcs_.push_back(rtp_params.flexfec.ssrc);
            }
            send_rtp_parameters_.emplace(std::move(rtp_params));
        }

        // Receivable
        if (local_media.direction() == sdp::Direction::RECV_ONLY ||
            local_media.direction() == sdp::Direction::SEND_RECV) {
            // TODO: Add video receive stream.
        }

    } else if (kind_ == Kind::AUDIO) {
        // TODO: Add audio send and receive stream.
    }
}

std::vector<uint32_t> MediaChannel::send_ssrcs() const {
    RTC_RUN_ON(worker_queue_);
    return send_ssrcs_;
}

void MediaChannel::OnRtcpPacket(CopyOnWriteBuffer in_packet) {
    RTC_RUN_ON(worker_queue_);
    if (send_stream_) {
        send_stream_->OnRtcpPacket(in_packet);
    }
}

void MediaChannel::OnRtpPacket(RtpPacketReceived in_packet) {
    RTC_RUN_ON(worker_queue_);
}

// Private methods
void MediaChannel::TriggerOpen() {
    RTC_RUN_ON(signaling_queue_);
    if (is_opened_) {
        return;
    }
    is_opened_ = true;
    if (opened_callback_) {
        opened_callback_();
    }
}

void MediaChannel::TriggerClose() {
    RTC_RUN_ON(signaling_queue_);
    if (!is_opened_) {
        return;
    }
    is_opened_ = false;
    if (closed_callback_) {
        closed_callback_();
    }
}

void MediaChannel::CreateStreams() {
    RTC_RUN_ON(worker_queue_);

    // Send stream
    if (send_rtp_parameters_) {
        VideoSendStream::Configuration send_config;
        send_config.clock = clock_.get();
        send_config.send_transport = send_transport_.lock().get();
        send_config.rtp = send_rtp_parameters_.value();

        send_stream_.reset(new VideoSendStream(std::move(send_config)));
    }

}

} // namespace naivertc