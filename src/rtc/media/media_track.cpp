#include "rtc/media/media_track.hpp"
#include "common/utils_random.hpp"
#include "rtc/call/call.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

MediaTrack::Kind ToKind(sdp::MediaEntry::Kind kind) {
    switch(kind) {
    case sdp::MediaEntry::Kind::AUDIO:
        return MediaTrack::Kind::AUDIO;
    case sdp::MediaEntry::Kind::VIDEO:
        return MediaTrack::Kind::VIDEO;
    default:
        RTC_NOTREACHED();
    }
}

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

// Media track
MediaTrack::MediaTrack(const Configuration& config,
                       Call* broadcaster,
                       TaskQueue* worker_queue) 
    : MediaTrack(SdpBuilder::Build(config), broadcaster, worker_queue) {}

MediaTrack::MediaTrack(sdp::Media description,
                       Call* broadcaster,
                       TaskQueue* worker_queue)
    : kind_(ToKind(description.kind())),
      description_(std::move(description)),
      call_(broadcaster),
      worker_queue_(worker_queue),
      signaling_queue_(TaskQueueImpl::Current()) {}

MediaTrack::~MediaTrack() {}

MediaTrack::Kind MediaTrack::kind() const {
    return kind_;
}

const std::string MediaTrack::mid() const {
    return description_.mid();
}

sdp::Media MediaTrack::description() const {
    return description_;
}

bool MediaTrack::is_opened() const {
    RTC_RUN_ON(signaling_queue_);
    return is_opened_;
}

void MediaTrack::OnOpened(OpenedCallback callback) {
    signaling_queue_->Post([this, callback=std::move(callback)](){
        opened_callback_ = callback;
        if (is_opened_ && opened_callback_) {
            opened_callback_();
        }
    });
}

void MediaTrack::OnClosed(ClosedCallback callback) {
    signaling_queue_->Post([this, callback=std::move(callback)](){
        closed_callback_ = callback;
    });
}

void MediaTrack::Open() {
    RTC_RUN_ON(signaling_queue_);
    TriggerOpen();
}

void MediaTrack::Close() {
    RTC_RUN_ON(signaling_queue_);
    TriggerClose();
    PLOG_VERBOSE << "Media channel closed.";
}

void MediaTrack::OnMediaNegotiated(sdp::Media local_media, 
                                   sdp::Media remote_media, 
                                   sdp::Type remote_sdp_type) {
    RTC_RUN_ON(worker_queue_);
    if (kind_ == Kind::VIDEO) {
        // Sendable
        if (local_media.direction() == sdp::Direction::SEND_ONLY ||
            local_media.direction() == sdp::Direction::SEND_RECV) {
            auto rtp_params = ParseRtpParameters(local_media);
            call_->AddVideoSendStream(std::move(rtp_params));
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

// Private methods
void MediaTrack::TriggerOpen() {
    RTC_RUN_ON(signaling_queue_);
    if (is_opened_) {
        return;
    }
    is_opened_ = true;
    if (opened_callback_) {
        opened_callback_();
    }
}

void MediaTrack::TriggerClose() {
    RTC_RUN_ON(signaling_queue_);
    if (!is_opened_) {
        return;
    }
    is_opened_ = false;
    if (closed_callback_) {
        closed_callback_();
    }
}


} // namespace naivertc