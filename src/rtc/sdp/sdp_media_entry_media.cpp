#include "rtc/sdp/sdp_media_entry_media.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace sdp {

// Media
Media::Media(const MediaEntry& entry, Direction direction)
    : MediaEntry(entry),
      direction_(direction) {}

Media::Media(MediaEntry&& entry, Direction direction) 
    : MediaEntry(entry),
      direction_(direction) {}

Media::Media(Kind kind, 
             std::string mid, 
             std::string protocols,
             Direction direction) 
    : MediaEntry(kind, std::move(mid), std::move(protocols)),
      direction_(direction),
      rtcp_mux_enabled_(false),
      rtcp_rsize_enabled_(false) {}

Media::~Media() = default;

// Ssrc entry
bool Media::IsMediaSsrc(uint32_t ssrc) const {
    return std::find(media_ssrcs_.begin(), media_ssrcs_.end(), ssrc) != media_ssrcs_.end();
}

bool Media::IsRtxSsrc(uint32_t ssrc) const {
    return std::find(rtx_ssrcs_.begin(), rtx_ssrcs_.end(), ssrc) != rtx_ssrcs_.end();
}

bool Media::IsFecSsrc(uint32_t ssrc) const {
    return std::find(fec_ssrcs_.begin(), fec_ssrcs_.end(), ssrc) != fec_ssrcs_.end();
}

Media::SsrcEntry* Media::AddSsrc(SsrcEntry entry) {
    if (entry.kind == SsrcEntry::Kind::RTX) {
        rtx_ssrcs_.emplace_back(entry.ssrc);
    } else if (entry.kind == SsrcEntry::Kind::FEC) {
        fec_ssrcs_.emplace_back(entry.ssrc);
    } else {
        media_ssrcs_.emplace_back(entry.ssrc);
    }
    return &(ssrc_entries_.emplace(entry.ssrc, std::move(entry)).first->second);
}

Media::SsrcEntry* Media::AddSsrc(uint32_t ssrc,
                                 SsrcEntry::Kind kind,
                                 std::optional<std::string> cname, 
                                 std::optional<std::string> msid, 
                                 std::optional<std::string> track_id) {
    return AddSsrc(SsrcEntry(ssrc, kind, cname, msid, track_id));
}

void Media::RemoveSsrc(uint32_t ssrc) {
    ssrc_entries_.erase(ssrc);
    auto it = std::find(media_ssrcs_.begin(), media_ssrcs_.end(), ssrc);
    if (it != media_ssrcs_.end()) {
        media_ssrcs_.erase(it);
    } else {
        it = std::find(rtx_ssrcs_.begin(), rtx_ssrcs_.end(), ssrc);
        if (it != rtx_ssrcs_.end()) {
            rtx_ssrcs_.erase(it);
        } else {
            it = std::find(fec_ssrcs_.begin(), fec_ssrcs_.end(), ssrc);
            if (it != fec_ssrcs_.end()) {
                fec_ssrcs_.erase(it);
            }
        }
    }
}

Media::SsrcEntry* Media::ssrc(uint32_t ssrc) {
    for (auto& kv : ssrc_entries_) {
        if (kv.first == ssrc) {
            return &kv.second;
        }
    }
    return nullptr;
}

const Media::SsrcEntry* Media::ssrc(uint32_t ssrc) const {
    for (const auto& kv : ssrc_entries_) {
        if (kv.first == ssrc) {
            return &kv.second;
        }
    }
    return nullptr;
}

Media::SsrcEntry::Kind Media::ssrc_kind(uint32_t ssrc) const {
    if (IsRtxSsrc(ssrc)) {
        return Media::SsrcEntry::Kind::RTX;
    } else if (IsFecSsrc(ssrc)) {
        return Media::SsrcEntry::Kind::FEC;
    } else {
        return Media::SsrcEntry::Kind::MEDIA;
    }
}

std::optional<uint32_t> Media::RtxSsrcAssociatedWithMediaSsrc(uint32_t ssrc) const {
    for (size_t i = 0; i < media_ssrcs_.size(); ++i) {
        if (i >= rtx_ssrcs_.size()) {
            break;
        }
        if (media_ssrcs_[i] == ssrc) {
            return rtx_ssrcs_[i];
        }
    }
    return std::nullopt;
}

std::optional<uint32_t> Media::FecSsrcAssociatedWithMediaSsrc(uint32_t ssrc) const {
    for (size_t i = 0; i < media_ssrcs_.size(); ++i) {
        if (i >= fec_ssrcs_.size()) {
            break;
        }
        if (media_ssrcs_[i] == ssrc) {
            return fec_ssrcs_[i];
        }
    }
    return std::nullopt;
}

void Media::ForEachSsrc(std::function<void(const SsrcEntry& ssrc_entry)>&& handler) const {
    for (auto& kv : ssrc_entries_) {
        handler(kv.second);
    }
}

void Media::ClearSsrcs() {
    media_ssrcs_.clear();
    rtx_ssrcs_.clear();
    fec_ssrcs_.clear();
    ssrc_entries_.clear();
}

bool Media::HasPayloadType(int pt) const {
    return rtp_maps_.find(pt) != rtp_maps_.end();
}

std::vector<int> Media::PayloadTypes() const {
    std::vector<int> payload_types;
    for (const auto& kv : rtp_maps_) {
        payload_types.push_back(kv.first);
    }
    return payload_types;
}

bool Media::AddFeedback(int payload_type, RtcpFeedback feed_back) {
    auto it = rtp_maps_.find(payload_type);
    if (it == rtp_maps_.end()) {
        PLOG_WARNING << "No RTP map found to add feedback with codec payload type: " << payload_type;
        return false;
    }
    it->second.rtcp_feedbacks.emplace_back(feed_back);
    return true;
}

Media::RtpMap* Media::AddCodec(int payload_type, 
                     Codec cocec,
                     int clock_rate,
                     std::optional<const std::string> codec_params,
                     std::optional<const std::string> profile) {
    RtpMap rtp_map(payload_type, cocec, clock_rate, codec_params);
    if (profile.has_value()) {
        rtp_map.fmt_profiles.emplace_back(profile.value());
    }
    return AddRtpMap(std::move(rtp_map));
}

Media::RtpMap* Media::AddRtpMap(RtpMap map) {
    auto [it, success] = rtp_maps_.emplace(map.payload_type, std::move(map));
    return success ? &(it->second) : nullptr;
}

void Media::ForEachRtpMap(std::function<void(const RtpMap& rtp_map)>&& handler) const {
    for (auto& kv : rtp_maps_) {
        if (handler) {
            handler(kv.second);
        }
    }
}

void Media::ClearRtpMap() {
    rtp_maps_.clear();
}

Media::ExtMap* Media::AddExtMap(int id, std::string uri) {
    return AddExtMap(ExtMap(id, uri));
}

Media::ExtMap* Media::AddExtMap(ExtMap ext_map) {
    auto [it, success] = ext_maps_.emplace(ext_map.id, std::move(ext_map));
    return success ? &(it->second) : nullptr;
}

bool Media::RemoveExtMap(int id) {
    return ext_maps_.erase(id) > 0;
}

bool Media::RemoveExtMap(std::string_view uri) {
    bool found = false;
    for (auto it = ext_maps_.begin(); it != ext_maps_.end(); ++it) {
        if (it->second.uri == uri) {
            found = true;
            ext_maps_.erase(it);
            break;
        }
    }
    return found;
}

void Media::ClearExtMap() {
    ext_maps_.clear();
}

void Media::ForEachExtMap(std::function<void(const ExtMap& ext_map)>&& handler) const {
    for (const auto& kv : ext_maps_) {
        if (handler) {
            handler(kv.second);
        }
    }
}

Media Media::Reciprocated() const {
    Media reciprocated(*this);

    // Invert direction
    switch (direction()) {
    case sdp::Direction::RECV_ONLY:
        reciprocated.set_direction(sdp::Direction::SEND_ONLY);
        break;
    case sdp::Direction::SEND_ONLY:
        reciprocated.set_direction(sdp::Direction::RECV_ONLY);
        break;
    default:
        // Keep the original direction
        break;
    }

    // Clear all SSRCs as them are individual
    // SSRC attributes are local and shouldn't be reciprocated
    // TODO: Attributes for remote SSRCs must be specified with the remote-ssrc SDP attribute.
    reciprocated.ClearSsrcs();

    return reciprocated;
}

// void Media::Reset() {
//     direction_ = Direction::INACTIVE;
//     rtcp_mux_enabled_ = false;
//     rtcp_rsize_enabled_ = false;
//     ext_maps_.clear();
//     rtp_maps_.clear();
//     ClearSsrcs();
//     bandwidth_max_value_ = -1;
// }

// Private methods
std::string Media::ToString(Codec codec) {
    switch (codec) {
    case sdp::Media::Codec::OPUS:
        return "OPUS";
    case sdp::Media::Codec::VP8:
        return "VP8";
    case sdp::Media::Codec::VP9:
        return "VP9";
    case sdp::Media::Codec::H264:
        return "H264";
    case sdp::Media::Codec::RED:
        return "red";
    case sdp::Media::Codec::ULP_FEC:
        return "ulpfec";
    case sdp::Media::Codec::FLEX_FEC:
        return "flexfec";
    case sdp::Media::Codec::RTX:
        return "rtx";
    }
}

std::string Media::ToString(RtcpFeedback rtcp_feedback) {
    switch (rtcp_feedback)
    {
    case RtcpFeedback::NACK:
        return "nack";
    case RtcpFeedback::GOOG_REMB:
        return "goog-remb";
    case RtcpFeedback::TRANSPORT_CC:
        return "transport-cc";
    }
}

} // namespace sdp
} // namespace naivert 