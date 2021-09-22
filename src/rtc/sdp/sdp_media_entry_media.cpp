#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "common/utils_string.hpp"

#include <plog/Log.h>

#include <sstream>

namespace naivertc {
namespace sdp {

Media::Media() 
    : MediaEntry(),
      direction_(Direction::UNKNOWN) {}

Media::Media(const MediaEntry& entry, Direction direction)
    : MediaEntry(entry),
      direction_(direction) {}

Media::Media(MediaEntry&& entry, Direction direction) 
    : MediaEntry(entry),
      direction_(direction) {}

Media::Media(Type type, 
             std::string mid, 
             const std::string protocols,
             Direction direction) 
    : MediaEntry(type, std::move(mid), protocols),
      direction_(direction) {}

std::string Media::MediaDescription() const {
    std::ostringstream desc;
    const std::string sp = " ";
    desc << MediaEntry::MediaDescription();
    
    for (auto it = rtp_map_.begin(); it != rtp_map_.end(); ++it) {
        desc << sp << it->first;
    }

    return desc.str();
}

std::string Media::GenerateSDPLines(const std::string eol) const {
    std::ostringstream oss;
    oss << MediaEntry::GenerateSDPLines(eol);

    switch(direction_) {
    case Direction::SEND_ONLY: 
        oss << "a=sendonly" << eol;
        break;
    case Direction::RECV_ONLY: 
        oss << "a=recvonly" << eol;
        break;
    case Direction::SEND_RECV: 
        oss << "a=sendrecv" << eol;
        break;
    case Direction::INACTIVE: 
        oss << "a=inactive" << eol;
        break;
    default:
        break;
    }

    oss << "a=rtcp-mux" << eol;

    for (auto it = rtp_map_.begin(); it != rtp_map_.end(); ++it) {
        auto &map = it->second;

        // a=rtpmap
        oss << "a=rtpmap:" << map.payload_type << ' ' << map.codec << "/" << map.clock_rate;
        if (!map.codec_params.has_value()) {
            oss << "/" << map.codec_params.value();
        }
        oss << eol;

        // a=rtcp-fb
        for (const auto& val : map.rtcp_feedbacks) {
            // TODO: Add transport-cc support
            if (val != "transport-cc") {
                oss << "a=rtcp-fb" << map.payload_type << ' ' << val << eol;
            }
        }

        // a=fmtp
        for (const auto& val : map.fmt_profiles) {
            oss << "a=fmtp:" << map.payload_type << ' ' << val << eol;
        }
    }

    return oss.str();
}

Media Media::reciprocate() const {
    Media reciprocated(*this);

    // Invert direction
    switch (direction())
    {
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

    return reciprocated;
}

void Media::AddSsrc(uint32_t ssrc, 
                    std::optional<std::string> cname, 
                    std::optional<std::string> msid, 
                    std::optional<std::string> track_id) {
    if (cname.has_value()) {
        attributes_.emplace_back("ssrc:" + std::to_string(ssrc) + " cname:" + cname.value());
    }else {
        attributes_.emplace_back("ssrc:" + std::to_string(ssrc));
    }

    if (msid.has_value()) {
        attributes_.emplace_back("ssrc:" + std::to_string(ssrc) + " msid:" + msid.value() + " " + track_id.value_or(*msid));
    }

    ssrcs_.emplace_back(ssrc);
}

void Media::RemoveSsrc(uint32_t ssrc) {
    for (auto it = attributes_.begin(); it != attributes_.end(); ++it) {
        if (utils::string::match_prefix(*it, "ssrc:" + std::to_string(ssrc))) {
            it = attributes_.erase(it);
        }
    }
    for (auto it = ssrcs_.begin(); it != ssrcs_.end(); ++it) {
        if (*it == ssrc) {
            it = ssrcs_.erase(it);
        }
    }
}

void Media::ReplaceSsrc(uint32_t old_ssrc, 
                        uint32_t ssrc, std::optional<std::string> name, 
                        std::optional<std::string> msid, 
                        std::optional<std::string> track_id) {
    RemoveSsrc(old_ssrc);
    AddSsrc(ssrc, std::move(name), std::move(msid), std::move(track_id));
} 

bool Media::HasSsrc(uint32_t ssrc) {
    return std::find(ssrcs_.begin(), ssrcs_.end(), ssrc) != ssrcs_.end();
}

std::optional<std::string> Media::CNameForSsrc(uint32_t ssrc) {
    auto it = cname_map_.find(ssrc);
    if (it != cname_map_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Media::HasPayloadType(int pt) const {
    return rtp_map_.find(pt) != rtp_map_.end();
}

void Media::AddFeedback(int payload_type, const std::string feed_back) {
    auto it = rtp_map_.find(payload_type);
    if (it == rtp_map_.end()) {
        it = rtp_map_.insert(std::make_pair(payload_type, RtpMap())).first;
    }
    it->second.rtcp_feedbacks.emplace_back(feed_back);
}

// Override
bool Media::ParseSDPLine(std::string_view line) {
    if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);

        auto [key, value] = utils::string::parse_pair(attr);
        return ParseSDPAttributeField(key, value);
        
    // 'b=AS', is used to negotiate the maximum bandwidth
    // eg: b=AS:80
    }else if (utils::string::match_prefix(line, "b=AS")) {
        bandwidth_max_value_ = utils::string::to_integer<int>(line.substr(line.find(':') + 1));
        return true;
    }else {
        return MediaEntry::ParseSDPLine(line);
    }
}

bool Media::ParseSDPAttributeField(std::string_view key, std::string_view value) {
    // Direction
    if (value == "sendonly") {
        direction_ = Direction::SEND_ONLY;
        return true;
    }else if (value == "recvonly") {
        direction_ = Direction::RECV_ONLY;
        return true;
    }else if (value == "sendrecv") {
        direction_ = Direction::SEND_RECV;
        return true;
    }else if (value == "inactive") {
        direction_ = Direction::INACTIVE;
        return true;
    }
    // eg: a=rtpmap:101 VP9/90000
    else if (key == "rtpmap") {
        auto rtp_map = Parse(value);
        if (rtp_map.has_value()) {
            auto it = rtp_map_.find(rtp_map->payload_type);
            if (it == rtp_map_.end()) {
                it = rtp_map_.insert(std::make_pair(rtp_map->payload_type, rtp_map.value())).first;
            }else {
                it->second.payload_type = rtp_map->payload_type;
                it->second.codec = rtp_map->codec;
                it->second.clock_rate = rtp_map->clock_rate;
                it->second.codec_params = rtp_map->codec_params;
            }
            return true;
        }else {
            return false;
        }
    }
    // eg: a=rtcp-fb:101 nack pli
    // eg: a=rtcp-fb:101 goog-remb
    else if (key == "rtcp-fb") {
        size_t sp = value.find(' ');
        int pt = utils::string::to_integer<int>(value.substr(0, sp));
        auto it = rtp_map_.find(pt);
        if (it == rtp_map_.end()) {
            it = rtp_map_.insert(std::make_pair(pt, RtpMap())).first;
        }
        it->second.rtcp_feedbacks.emplace_back(value.substr(sp + 1));
        return true;
    }
    // eg: a=fmtp:107 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
    else if (key == "fmtp") {
        size_t sp = value.find(' ');
        int pt = utils::string::to_integer<int>(value.substr(0, sp));
        auto it = rtp_map_.find(pt);
        if (it == rtp_map_.end()) {
            it = rtp_map_.insert(std::make_pair(pt, RtpMap())).first;
        }
        it->second.fmt_profiles.emplace_back(value.substr(sp + 1));
        return true;
    }else if (value == "rtcp-mux") {
        // Added by default
        return true;
    }else if (key == "rtcp") {
        // Added by default
        return true;
    }
    // eg: a=ssrc:3463951252 cname:sTjtznXLCNH7nbRw
    else if (key == "ssrc") {
        auto ssrc = utils::string::to_integer<uint32_t>(value);
        if (!HasSsrc(ssrc)) {
            ssrcs_.emplace_back(ssrc);
        }
        auto cname_pos = value.find("cname:");
        if (cname_pos != std::string::npos) {
            auto cname = value.substr(cname_pos + 6);
            cname_map_.emplace(ssrc, cname);
        }
        attributes_.emplace_back("ssrc:" + std::string(value));
        return true;
    }
    // TODO: Support more attributes
    else if (key == "extmap" ||
                key == "rtcp-rsize") {
        attributes_.emplace_back(std::string(value));
        return true;
    }
    else {
        return MediaEntry::ParseSDPAttributeField(key, value);
    }
}

void Media::AddRtpMap(const RtpMap& map) {
    rtp_map_.emplace(map.payload_type, map);
}

// [key]:[value]
// a=rtpmap:102 H264/90000
std::optional<Media::RtpMap> Media::Parse(const std::string_view& attr_value) {
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

    auto codec = line.substr(0, spl);

    line = line.substr(spl + 1);
    spl = line.find('/');

    if (spl == std::string::npos) {
        spl = line.find(' ');
    }

    Media::RtpMap rtp_map;
    rtp_map.payload_type = payload_type;
    rtp_map.codec = std::move(codec);

    if (spl == std::string::npos) {
        rtp_map.clock_rate = utils::string::to_integer<int>(line);
    }else {
        rtp_map.clock_rate = utils::string::to_integer<int>(line.substr(0, spl));
        rtp_map.codec_params = line.substr(spl + 1);
    }

    return rtp_map;
}

} // namespace sdp
} // namespace naivert 