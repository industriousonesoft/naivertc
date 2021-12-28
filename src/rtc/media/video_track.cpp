#include "rtc/media/video_track.hpp"

namespace naivertc {

VideoTrack::~VideoTrack() {}

VideoSendStream::Configuration VideoTrack::BuildSendConfig(const sdp::Media& description) const {
    VideoSendStream::Configuration send_stream_config;
  
    auto media_ssrcs = description.media_ssrcs();
    auto rtx_ssrcs = description.rtx_ssrcs();
    auto fec_ssrcs = description.fec_ssrcs();

    // Ssrcs
    // Media ssrc
    if (!media_ssrcs.empty()) {
        send_stream_config.rtp.local_media_ssrc = media_ssrcs[0];
    }
    // Rtx ssrc
    if (!rtx_ssrcs.empty()) {
        send_stream_config.rtp.rtx_send_ssrc = rtx_ssrcs[0];
    }
    // FlexFec ssrc
    if (!fec_ssrcs.empty()) {
        send_stream_config.rtp.flexfec.ssrc = fec_ssrcs[0];
    }

    // Payload types
    description.ForEachRtpMap([&](const sdp::Media::RtpMap& rtp_map){
        switch (rtp_map.codec)
        {
        case sdp::Media::Codec::H264: {
            send_stream_config.rtp.media_payload_type = rtp_map.payload_type;
            send_stream_config.rtp.media_rtx_payload_type = rtp_map.rtx_payload_type;
            for (const auto& rtcp_feedback : rtp_map.rtcp_feedbacks) {
                // NACK enabled
                if (rtcp_feedback == sdp::Media::RtcpFeedback::NACK) {
                    send_stream_config.rtp.nack_enabled = true;
                }
            }
            break;
        }
        case sdp::Media::Codec::RED: {
            send_stream_config.rtp.ulpfec.red_payload_type = rtp_map.payload_type;
            send_stream_config.rtp.ulpfec.red_rtx_payload_type = rtp_map.rtx_payload_type;
            break;
        }
        case sdp::Media::Codec::ULP_FEC: {
            send_stream_config.rtp.ulpfec.ulpfec_payload_type = rtp_map.payload_type;
            break;
        }
        default:
            // TODO: Support more codecs.
            break;
        }
    });

    return send_stream_config;
}
    
} // namespace naivertc
