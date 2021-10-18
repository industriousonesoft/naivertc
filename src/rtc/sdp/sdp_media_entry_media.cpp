#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "common/utils_string.hpp"

#include <plog/Log.h>

#include <sstream>

namespace naivertc {
namespace sdp {
// RtpMap
Media::RtpMap::RtpMap(int payload_type, 
                     std::string codec, 
                     int clock_rate, 
                     std::optional<std::string> codec_params) 
    : payload_type(payload_type),
      codec(std::move(codec)),
      clock_rate(clock_rate),
      codec_params(codec_params) {}

// SsrcEntry
Media::SsrcEntry::SsrcEntry(uint32_t ssrc,
                            Kind kind,
                            std::optional<std::string> cname, 
                            std::optional<std::string> msid, 
                            std::optional<std::string> track_id) 
    : ssrc(ssrc),
      kind(kind),
      cname(cname),
      msid(msid),
      track_id(track_id) {}

// Media
Media::Media() 
    : MediaEntry(),
      direction_(Direction::INACTIVE) {}

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
      direction_(direction) {}

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

std::vector<int> Media::payload_types() const {
    std::vector<int> payload_types;
    for (const auto& kv : rtp_maps_) {
        payload_types.push_back(kv.first);
    }
    return payload_types;
}

bool Media::AddFeedback(int payload_type, const std::string feed_back) {
    auto it = rtp_maps_.find(payload_type);
    if (it == rtp_maps_.end()) {
        PLOG_WARNING << "No RTP map found to add feedback with payload type: " << payload_type;
        return false;
    }
    it->second.rtcp_feedbacks.emplace_back(feed_back);
    return true;
}

void Media::AddRtpMap(RtpMap map) {
    rtp_maps_.emplace(map.payload_type, std::move(map));
}

void Media::AddCodec(int payload_type, 
                     const std::string codec,
                     int clock_rate,
                     std::optional<const std::string> codec_params,
                     std::optional<const std::string> profile) {
    RtpMap rtp_map(payload_type, codec, clock_rate, codec_params);
    if (profile.has_value()) {
        rtp_map.fmt_profiles.emplace_back(profile.value());
    }
    AddRtpMap(rtp_map);
}

void Media::AddExtraAttribute(std::string attr_value) {
    extra_attributes_.push_back(std::move(attr_value));
}

Media Media::ReciprocatedSDP() const {
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
//     rtp_maps_.clear();
//     ClearSsrcs();
//     extra_attributes_.clear();
//     bandwidth_max_value_ = -1;
// }

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
    // eg: a=rtpmap:101 VP9/90000
    else if (key == "rtpmap") {
        auto rtp_map = ParseRtpMap(value);
        if (rtp_map.has_value()) {
            auto it = rtp_maps_.find(rtp_map->payload_type);
            if (it == rtp_maps_.end()) {
                it = rtp_maps_.insert(std::make_pair(rtp_map->payload_type, rtp_map.value())).first;
            } else {
                it->second.payload_type = rtp_map->payload_type;
                it->second.codec = rtp_map->codec;
                it->second.clock_rate = rtp_map->clock_rate;
                if (rtp_map->codec_params.has_value()) {
                    it->second.codec_params.emplace(it->second.codec_params.value());
                }
            }
            return true;
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
        if (it == rtp_maps_.end()) {
            PLOG_WARNING << "No RTP map found before parsing 'rtcp-fb' with payload type: " << payload_type;
            return false;
        }
        it->second.rtcp_feedbacks.emplace_back(value.substr(sp + 1));
        return true;
    }
    // eg: a=fmtp:107 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
    else if (key == "fmtp") {
        size_t sp = value.find(" ");
        int payload_type = utils::string::to_integer<int>(value.substr(0, sp));
        auto it = rtp_maps_.find(payload_type);
        if (it == rtp_maps_.end()) {
            PLOG_WARNING << "No RTP map found before parsing 'fmtp' with payload type: " << payload_type;
            return false;
        }
        it->second.fmt_profiles.emplace_back(value.substr(sp + 1));
        return true;
    } else if (value == "rtcp-mux") {
        // Added by default
        return true;
    } else if (key == "rtcp") {
        // Added by default
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
    // TODO: Support more attributes
    else if (key == "extmap" ||
                key == "rtcp-rsize") {
        extra_attributes_.push_back(std::string(value));
        return true;
    }
    else {
        return MediaEntry::ParseSDPAttributeField(key, value);
    }
}

// Private methods
std::string Media::FormatDescription() const {
    std::ostringstream desc;
    const std::string sp = " ";
    for (const auto& kv : rtp_maps_) {
        desc << sp << kv.first;
    }
    return desc.str().substr(1 /* Trim the first space */);
}

std::string Media::GenerateSDPLines(const std::string eol) const {
    std::ostringstream oss;
    const std::string sp = " ";
    oss << MediaEntry::GenerateSDPLines(eol);

    // a=sendrecv
    oss << "a=" << direction_ << eol;

    // Rtp and Rtcp share the same socket and connection
    // instead of using two separate connections.
    oss << "a=rtcp-mux" << eol;

    for (const auto& [key, map] : rtp_maps_) {
        // a=rtpmap
        oss << "a=rtpmap:" << map.payload_type << sp << map.codec << "/" << map.clock_rate;
        if (map.codec_params.has_value()) {
            oss << "/" << map.codec_params.value();
        }
        oss << eol;

        // a=rtcp-fb
        for (const auto& val : map.rtcp_feedbacks) {
            if (val != "transport-cc") {
                oss << "a=rtcp-fb:" << map.payload_type << sp << val << eol;
            }
        }

        // a=fmtp
        for (const auto& val : map.fmt_profiles) {
            oss << "a=fmtp:" << map.payload_type << sp << val << eol;
        }
    }

    // a=ssrc
    for (const auto& ssrc : media_ssrcs_) {
        std::optional<uint32_t> associated_rtx_ssrc = RtxSsrcAssociatedWithMediaSsrc(ssrc);
        std::optional<uint32_t> associated_fec_ssrc = FecSsrcAssociatedWithMediaSsrc(ssrc);
    
        // No associated ssrc
        if (!associated_rtx_ssrc && !associated_rtx_ssrc) {
            // a=ssrc
            // Media ssrc entry
            oss << GenerateSsrcEntrySDPLines(ssrc_entries_.at(ssrc), eol);
        } else {
            // a=ssrc-group:FID
            if (associated_rtx_ssrc) {
                oss << "a=ssrc-group:FID" << sp << ssrc << sp << associated_rtx_ssrc.value() << eol;
                // a=ssrc
                // Media ssrc entry
                oss << GenerateSsrcEntrySDPLines(ssrc_entries_.at(ssrc), eol);
                // RTX ssrc entry
                oss << GenerateSsrcEntrySDPLines(ssrc_entries_.at(associated_rtx_ssrc.value()), eol);

            }
            // a=ssrc-group:FEC
            if (associated_fec_ssrc) {
                oss << "a=ssrc-group:FEC" << sp << ssrc << sp << associated_rtx_ssrc.value() << eol;
                // a=ssrc
                // Media ssrc entry
                oss << GenerateSsrcEntrySDPLines(ssrc_entries_.at(ssrc), eol);
                // FEC ssrc entry
                oss << GenerateSsrcEntrySDPLines(ssrc_entries_.at(associated_fec_ssrc.value()), eol);
            }
        }
    }
    
    // Extra attributes
    for (const auto& attr : extra_attributes_) {
        // extmap：表示rtp报头拓展的映射，可能有多个，eg: a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
        // rtcp-resize(rtcp reduced size), 表示rtcp包是使用固定算法缩小过的
        // FIXME: Add rtcp-rsize support
        if (attr.find("extmap") == std::string::npos && attr.find("rtcp-rsize") == std::string::npos) {
            oss << "a=" << attr << eol;
        }
    }

    return oss.str();
}

std::string Media::GenerateSsrcEntrySDPLines(const SsrcEntry& entry, const std::string eol) const {
    std::ostringstream oss;
    const std::string sp = " ";
    if (entry.cname.has_value()) {
        oss << "a=ssrc:" << entry.ssrc << sp 
            << "cname:" << entry.cname.value() << eol;;
    } else {
        oss << "a=ssrc:" << entry.ssrc << eol;;
    }

    if (entry.msid.has_value()) {
        oss << "a=ssrc:" << entry.ssrc << sp 
            << "msid:" << entry.msid.value() << sp 
            << entry.track_id.value_or(entry.msid.value()) << eol;;
    }
    return oss.str();
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

    auto codec = std::string(line.substr(0, spl));

    line = line.substr(spl + 1);
    spl = line.find('/');

    if (spl == std::string::npos) {
        spl = line.find(' ');
    }
  
    int clock_rate = -1;
    std::optional<std::string> codec_params;
    if (spl == std::string::npos) {
        clock_rate = utils::string::to_integer<int>(line);
    } else {
        clock_rate = utils::string::to_integer<int>(line.substr(0, spl));
        codec_params.emplace(line.substr(spl + 1));
    }

    return RtpMap(payload_type, std::move(codec), clock_rate, codec_params);
}

} // namespace sdp
} // namespace naivert 