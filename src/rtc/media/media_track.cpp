#include "rtc/media/media_track.hpp"
#include "common/utils_random.hpp"
#include "rtc/call/call.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"

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
    // Rtx ssrc
    if (!media.rtx_ssrcs().empty()) {
        rtp_parameters.rtx_send_ssrc = media.rtx_ssrcs()[0];
    }
    // FlexFec ssrc
    if (!media.fec_ssrcs().empty()) {
        rtp_parameters.flexfec.ssrc = media.fec_ssrcs()[0];
    }

    // extensions
    media.ForEachExtMap([&](const sdp::Media::ExtMap& ext_map){
        rtp_parameters.extensions.emplace_back(ext_map.id, ext_map.uri);
    });
    
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

void MediaTrack::OnNegotiated(const sdp::Description& local_sdp, 
                              const sdp::Description& remote_sdp) {
    RTC_RUN_ON(worker_queue_);
    auto mid = this->mid();
    const sdp::Media* local_media = local_sdp.media(mid);
    const sdp::Media* remote_media = remote_sdp.media(mid);
    if (kind_ == Kind::VIDEO) {
        // Sendable
        if (local_media->direction() == sdp::Direction::SEND_ONLY ||
            local_media->direction() == sdp::Direction::SEND_RECV) {
            // Add video send stream.
            if (!local_media->media_ssrcs().empty()) {
                auto rtp_params = ParseRtpParameters(*local_media);
                // Local media SSRC.
                rtp_params.local_media_ssrc = local_media->media_ssrcs()[0];
                // Don't care remote media SSRC.
                rtp_params.remote_media_ssrc = std::nullopt;
                rtp_params.extmap_allow_mixed = local_sdp.extmap_allow_mixed();
                call_->AddVideoSendStream(rtp_params);
            } else {
                PLOG_WARNING << "Failed to add video send stream as no media stream found.";
            }
        }

        // Receivable
        if (local_media->direction() == sdp::Direction::RECV_ONLY ||
            local_media->direction() == sdp::Direction::SEND_RECV) {
            // Add video receive stream.
            if (!remote_media->media_ssrcs().empty()) {
                auto rtp_params = ParseRtpParameters(*remote_media);
                // Local media SSRC.
                if (!local_media->media_ssrcs().empty()) {
                    rtp_params.local_media_ssrc = local_media->media_ssrcs()[0];
                } else {
                    rtp_params.local_media_ssrc = 1; // Receive only.
                }
                // Remote media SSRC.
                rtp_params.remote_media_ssrc = remote_media->media_ssrcs()[0];
                rtp_params.extmap_allow_mixed = local_sdp.extmap_allow_mixed();
                call_->AddVideoRecvStream(rtp_params);
            }
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