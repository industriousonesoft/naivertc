#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "common/utils_string.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace sdp {
namespace {

std::optional<sdp::Media::Codec> CodecFromString(const std::string_view& codec_name) {
    if (codec_name == "OPUS" || codec_name == "opus") {
        return sdp::Media::Codec::OPUS;
    } else if (codec_name == "VP8" || codec_name == "vp8") {
        return sdp::Media::Codec::VP8;
    } else if (codec_name == "VP9" || codec_name == "vp9") {
        return sdp::Media::Codec::VP9;
    } else if (codec_name == "H264" || codec_name == "h264") {
        return sdp::Media::Codec::H264;
    } else if (codec_name == "RED" || codec_name == "red") {
        return sdp::Media::Codec::RED;
    } else if (codec_name == "ULPFEC" || codec_name == "ulpfec") {
        return sdp::Media::Codec::ULP_FEC;
    } else if (codec_name == "FLEXFEC" || codec_name == "flexfec") {
        return sdp::Media::Codec::FLEX_FEC;
    } else if (codec_name == "RTX" || codec_name == "rtx") {
        return sdp::Media::Codec::RTX;
    } else {
        PLOG_WARNING << "Unsupport codec: " << codec_name;
        return std::nullopt;
    }
}

std::optional<sdp::Media::RtcpFeedback> RtcpFeedbackFromString(const std::string_view& feedback_name) {
    if (feedback_name == "NACK" || feedback_name == "nack") {
        return sdp::Media::RtcpFeedback::NACK;
    } else if (feedback_name == "GOOG-REMB" || feedback_name == "goog-remb") {
        return sdp::Media::RtcpFeedback::GOOG_REMB;
    } else if (feedback_name == "TRANSPORT-CC" || feedback_name == "transport-cc") {
        return sdp::Media::RtcpFeedback::TRANSPORT_CC;
    } else {
        PLOG_WARNING << "Unsupport RTCP feedback: " << feedback_name;
        return std::nullopt;
    }
}

} // namespace

// Override
bool Media::ParseSDPLine(std::string_view line) {
    if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);

        auto [key, value] = utils::string::parse_pair(attr);
        return ParseSDPAttributeField(key, value);
        
    // 'b=AS', is used to negotiate the maximum bandwidth
    // eg: b=AS:80
    } else if (utils::string::match_prefix(line, "b=AS")) {
        bandwidth_max_value_ = utils::string::to_integer<int>(line.substr(line.find(':') + 1));
        return true;
    } else {
        return MediaEntry::ParseSDPLine(line);
    }
}

bool Media::ParseSDPAttributeField(std::string_view key, std::string_view value) {
    // Direction
    if (value == "sendonly") {
        direction_ = Direction::SEND_ONLY;
        return true;
    } else if (value == "recvonly") {
        direction_ = Direction::RECV_ONLY;
        return true;
    } else if (value == "sendrecv") {
        direction_ = Direction::SEND_RECV;
        return true;
    } else if (value == "inactive") {
        direction_ = Direction::INACTIVE;
        return true;
    }
    // eg: a=extmap:14 urn:ietf:params:rtp-hdrext:toffset
    else if (key == "extmap") {
        size_t sp = value.find(" ");
        int ext_id = utils::string::to_integer<int>(value.substr(0, sp));
        // The range of extension id is [1, 255];
        assert(ext_id > 0 && ext_id < 256);
        auto ext_uri = std::string(value.substr(sp + 1));
        auto it = ext_maps_.find(ext_id);
        if (it != ext_maps_.end()) {
            PLOG_WARNING << "a=extmap:" << it->first << " " << it->second.uri
                         << "is replaced with a=extmap:" << it->first << " " << ext_uri;
            it->second.uri = ext_uri;
            return true;
        } else {
            return ext_maps_.insert({ext_id, ExtMap(ext_id, ext_uri)}).second;
        }
    }
    // eg: a=rtpmap:101 VP9/90000
    else if (key == "rtpmap") {
        auto rtp_map = ParseRtpMap(value);
        if (rtp_map.has_value()) {
            auto it = rtp_maps_.find(rtp_map->payload_type);
            if (it == rtp_maps_.end()) {
                return rtp_maps_.insert({rtp_map->payload_type, std::move(rtp_map.value())}).second;
            } else {
                it->second.payload_type = rtp_map->payload_type;
                it->second.codec = rtp_map->codec;
                it->second.clock_rate = rtp_map->clock_rate;
                if (rtp_map->codec_params.has_value()) {
                    it->second.codec_params.emplace(it->second.codec_params.value());
                }
                return true;
            }
        } else {
            return false;
        }
    }
    // eg: a=rtcp-fb:101 nack pli
    // eg: a=rtcp-fb:101 goog-remb
    else if (key == "rtcp-fb") {
        size_t sp = value.find(' ');
        int payload_type = utils::string::to_integer<int>(value.substr(0, sp));
        auto it = rtp_maps_.find(payload_type);
        // The 'rtcp-fb' should be parsed after 'rtpmap'.
        if (it == rtp_maps_.end()) {
            PLOG_WARNING << "No RTP map found before parsing 'rtcp-fb' with payload type: " << payload_type;
            return false;
        }
        auto rtcp_feedback = RtcpFeedbackFromString(value.substr(sp + 1));
        if (rtcp_feedback) {
            it->second.rtcp_feedbacks.emplace_back(*rtcp_feedback);
        }
        return true;
    }
    // eg: a=fmtp:101 apt=100
    // eg: a=fmtp:107 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
    else if (key == "fmtp") {
        size_t sp = value.find(" ");
        int payload_type = utils::string::to_integer<int>(value.substr(0, sp));
        auto it = rtp_maps_.find(payload_type);
        // The 'fmtp' should be parsed after 'rtpmap'.
        if (it == rtp_maps_.end()) {
            PLOG_WARNING << "No RTP map found before parsing 'fmtp' with payload type: " << payload_type;
            return false;
        }
        it->second.fmt_profiles.emplace_back(value.substr(sp + 1));
        return true;
    }
    // a=ssrc-group:<semantics> <ssrc-id>
    // eg: a=ssrc-group:FID 3463951252 1461041037
    // eg: a=ssrc-group:FEC 3463951252 1461041037
    else if (key == "ssrc-group") {
        size_t sp = value.find(" ");
        auto semantics = value.substr(0, sp);
        std::vector<uint32_t> ssrcs;
        auto ssrc_id_str = value.substr(sp + 1);
        sp = ssrc_id_str.find(" ");
        // Media ssrc
        auto media_ssrc = utils::string::to_integer<uint32_t>(ssrc_id_str.substr(0, sp));
        media_ssrcs_.emplace_back(media_ssrc);

        // Associated ssrc
        auto associated_ssrc = utils::string::to_integer<uint32_t>(ssrc_id_str.substr(sp + 1));
        if (semantics == "FID") {
            rtx_ssrcs_.emplace_back(associated_ssrc);
        } else if (semantics == "FEC") {
            fec_ssrcs_.emplace_back(associated_ssrc);
        } else {
            // TODO: How to handle SIM(simulcate) streams?
            media_ssrcs_.emplace_back(media_ssrc);
        }
        return true;
    }
    // eg: a=ssrc:3463951252 cname:sTjtznXLCNH7nbRw
    else if (key == "ssrc") {
        auto ssrc = utils::string::to_integer<uint32_t>(value);
        auto it = ssrc_entries_.find(ssrc);
        if (it == ssrc_entries_.end()) {
            if (IsRtxSsrc(ssrc)) {
                it = ssrc_entries_.emplace(ssrc, SsrcEntry(ssrc, SsrcEntry::Kind::RTX)).first;
            } else if (IsFecSsrc(ssrc)) {
                it = ssrc_entries_.emplace(ssrc, SsrcEntry(ssrc, SsrcEntry::Kind::FEC)).first;
            } else {
                it = ssrc_entries_.emplace(ssrc, SsrcEntry(ssrc, SsrcEntry::Kind::MEDIA)).first;
                // In case of no 'ssrc-group'
                if (!IsMediaSsrc(ssrc)) {
                    media_ssrcs_.emplace_back(ssrc);
                }
            }
        }
        auto cname_pos = value.find("cname:");
        if (cname_pos != std::string::npos) {
            auto cname = value.substr(cname_pos + 6);
            it->second.cname = cname;
        }
        auto msid_pos = value.find("msid:");
        if (msid_pos != std::string::npos) {
            auto msid_str = value.substr(msid_pos + 5);
            auto track_id_pos = msid_str.find(" ");
            if (track_id_pos != std::string::npos) {
                auto msid = msid_str.substr(0, track_id_pos);
                auto track_id = msid_str.substr(track_id_pos + 1);
                it->second.msid = msid;
                it->second.track_id = track_id;
            } else {
                it->second.msid = msid_str;
            }
        }
        return true;
    }
    else {
        if (value == "rtcp-mux") {
            rtcp_mux_enabled_ = true;
            return true;
        } else if (key == "rtcp-rsize") {
            rtcp_rsize_enabled_ = true;
            return true;
        }
        return MediaEntry::ParseSDPAttributeField(key, value);
    }
}

// [key]:[value]
// a=rtpmap:102 H264/90000
std::optional<Media::RtpMap> Media::ParseRtpMap(const std::string_view& attr_value) {
    size_t p = attr_value.find(' ');
    if (p == std::string::npos) {
        PLOG_WARNING << "No payload type found in attribure line: " << attr_value;
        return std::nullopt;
    }

    int payload_type = utils::string::to_integer<int>(attr_value.substr(0, p));

    std::string_view line = attr_value.substr(p + 1);
    // find separator line
    size_t spl = line.find('/');
    if (spl == std::string::npos) {
        PLOG_WARNING << "No codec type found in attribure line: " << attr_value;
        return std::nullopt;
    }

    auto codec_name = std::string(line.substr(0, spl));
    auto codec = CodecFromString(codec_name);

    // Unsupported codec type.
    if (!codec) {
        return std::nullopt;
    }

    line = line.substr(spl + 1);
    spl = line.find('/');

    if (spl == std::string::npos) {
        spl = line.find(' ');
    }
  
    // clock_rate
    int clock_rate = -1;
    std::optional<std::string> codec_params;
    if (spl == std::string::npos) {
        clock_rate = utils::string::to_integer<int>(line);
    } else {
        clock_rate = utils::string::to_integer<int>(line.substr(0, spl));
        codec_params.emplace(line.substr(spl + 1));
    }

    return RtpMap(payload_type, *codec, clock_rate, codec_params);
}

} // namespace sdp
} // namespace naivertc