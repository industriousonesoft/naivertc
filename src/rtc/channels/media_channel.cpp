#include "rtc/channels/media_channel.hpp"
#include "rtc/media/video_send_stream.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

std::string ToString(MediaChannel::Kind kind) {
    switch(kind) {
    case MediaChannel::Kind::AUDIO:
        return "audio";
    case MediaChannel::Kind::VIDEO:
        return "video";
    default:
        RTC_NOTREACHED();
    }
}

void ParseRtpSendParameters(const sdp::Media& media, VideoSendStream::Configuration& config) {
    // Ssrcs
    // Media ssrc
    if (!media.media_ssrcs().empty()) {
        config.rtp.local_media_ssrc = media.media_ssrcs()[0];
    }
    // Rtx ssrc
    if (!media.rtx_ssrcs().empty()) {
        config.rtp.rtx_send_ssrc = media.rtx_ssrcs()[0];
    }
    // FlexFec ssrc
    if (!media.fec_ssrcs().empty()) {
        config.rtp.flexfec.ssrc = media.fec_ssrcs()[0];
    }
    
    // Payload types
    media.ForEachRtpMap([&](const sdp::Media::RtpMap& rtp_map){
        switch (rtp_map.codec)
        {
        case sdp::Media::Codec::H264: {
            config.rtp.media_payload_type = rtp_map.payload_type;
            config.rtp.media_rtx_payload_type = rtp_map.rtx_payload_type;
            for (const auto& rtcp_feedback : rtp_map.rtcp_feedbacks) {
                // NACK enabled
                if (rtcp_feedback == sdp::Media::RtcpFeedback::NACK) {
                    config.rtp.nack_enabled = true;
                }
            }
            break;
        }
        case sdp::Media::Codec::RED: {
            config.rtp.ulpfec.red_payload_type = rtp_map.payload_type;
            config.rtp.ulpfec.red_rtx_payload_type = rtp_map.rtx_payload_type;
            break;
        }
        case sdp::Media::Codec::ULP_FEC: {
            config.rtp.ulpfec.ulpfec_payload_type = rtp_map.payload_type;
            break;
        }
        default:
            // TODO: Support more codecs.
            break;
        }
    });
}

    
} // namespace


MediaChannel::MediaChannel(Kind kind, std::string mid)
    : kind_(kind),
      mid_(std::move(mid)),
      clock_(std::make_unique<RealTimeClock>()),
      signaling_queue_(TaskQueueImpl::Current()) {
    worker_queue_checker_.Detach();
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
        TriggerOpen();
    });
}

void MediaChannel::OnClosed(ClosedCallback callback) {
    signaling_queue_->Post([this, callback=std::move(callback)](){
        closed_callback_ = callback;
        TriggerClose();
    });
}

void MediaChannel::Open(std::weak_ptr<MediaTransport> transport) {
    RTC_RUN_ON(signaling_queue_);
    send_transport_ = std::move(transport);
}

void MediaChannel::Close() {
    RTC_RUN_ON(signaling_queue_);
    send_transport_.reset();
}

void MediaChannel::OnMediaNegotiated(sdp::Media local_media, 
                                     sdp::Media remote_media, 
                                     sdp::Type remote_sdp_type) {
    RTC_RUN_ON(&worker_queue_checker_);
   
    if (kind_ == Kind::VIDEO) {
        // Sendable
        if (local_media.direction() == sdp::Direction::SEND_ONLY ||
            local_media.direction() == sdp::Direction::SEND_RECV) {
            VideoSendStream::Configuration send_config;
            send_config.clock = clock_.get();
            send_config.send_transport = send_transport_.lock().get();
        
            ParseRtpSendParameters(local_media, send_config);

            // Media ssrc
            if (send_config.rtp.local_media_ssrc >= 0) {
                send_ssrcs_.push_back(send_config.rtp.local_media_ssrc);
            }
            // RTX ssrc
            if (send_config.rtp.rtx_send_ssrc) {
                send_ssrcs_.push_back(*send_config.rtp.rtx_send_ssrc);
            }
            // FLEX_FEC ssrc
            if (send_config.rtp.flexfec.payload_type >= 0) {
                send_ssrcs_.push_back(send_config.rtp.flexfec.ssrc);
            }

            SendQueue()->Async([this, config=std::move(send_config)](){
                send_stream_ = std::unique_ptr<MediaSendStream>(new VideoSendStream(std::move(config), SendQueue()));
            });
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
    RTC_RUN_ON(&worker_queue_checker_);
    return send_ssrcs_;
}

void MediaChannel::OnRtcpPacket(CopyOnWriteBuffer in_packet) {
    RTC_RUN_ON(&worker_queue_checker_);
}

void MediaChannel::OnRtpPacket(RtpPacketReceived in_packet) {
    RTC_RUN_ON(&worker_queue_checker_);
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

TaskQueue* MediaChannel::SendQueue() {
    if (!send_queue_) {
        send_queue_ = std::make_unique<TaskQueue>(ToString(kind_) + ".channel.send.queue");
    }
    return send_queue_.get();
}

} // namespace naivertc