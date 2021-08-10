#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "common/utils_string.hpp"

#include <sstream>

namespace naivertc {
namespace sdp {

Media::Media(const std::string& sdp) 
    : MediaEntry(sdp, ""),
    direction_(Direction::UNKNOWN) {}
    
Media::Media(const std::string& mline, const std::string mid, Direction direction) 
    : MediaEntry(mline, std::move(mid)),
    direction_(direction) {}

Direction Media::direction() const {
    return direction_;
}

void Media::set_direction(Direction direction) {
    direction_ = direction;
}

std::string Media::description() const {
    std::ostringstream desc;
    desc << MediaEntry::description();
    
    for (auto it = rtp_map_.begin(); it != rtp_map_.end(); ++it) {
        desc << ' ' << it->first;
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
        oss << "a=rtpmap:" << map.pt << ' ' << map.format << "/" << map.clock_rate;
        if (!map.codec_params.empty()) {
            oss << "/" << map.codec_params;
        }
        oss << eol;

        // a=rtcp-fb
        for (const auto& val : map.rtcp_feedbacks) {
            // TODO: Add transport-cc support
            if (val != "transport-cc") {
                oss << "a=rtcp-fb" << map.pt << ' ' << val << eol;
            }
        }

        // a=fmtp
        for (const auto& val : map.fmt_profiles) {
            oss << "a=fmtp:" << map.pt << ' ' << val << eol;
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

void Media::AddSSRC(uint32_t ssrc, std::optional<std::string> name, std::optional<std::string> msid, std::optional<std::string> track_id) {
    if (name) {
        attributes_.emplace_back("ssrc:" + std::to_string(ssrc) + " cname:" + *name);
    }else {
        attributes_.emplace_back("ssrc:" + std::to_string(ssrc));
    }

    if (msid) {
        attributes_.emplace_back("ssrc:" + std::to_string(ssrc) + " msid:" + *msid + " " + track_id.value_or(*msid));
    }

    ssrcs_.emplace_back(ssrc);
}

void Media::RemoveSSRC(uint32_t ssrc) {
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

void Media::ReplaceSSRC(uint32_t old_ssrc, uint32_t ssrc, std::optional<std::string> name, std::optional<std::string> msid, std::optional<std::string> track_id) {
    RemoveSSRC(old_ssrc);
    AddSSRC(ssrc, std::move(name), std::move(msid), std::move(track_id));
} 

bool Media::HasSSRC(uint32_t ssrc) {
    return std::find(ssrcs_.begin(), ssrcs_.end(), ssrc) != ssrcs_.end();
}

std::vector<uint32_t> Media::GetSSRCS() {
    return ssrcs_;
}

std::optional<std::string> Media::GetCNameForSSRC(uint32_t ssrc) {
    auto it = cname_map_.find(ssrc);
    if (it != cname_map_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Media::set_bandwidth_max_value(int value) {
    bandwidth_max_value_ = value;
}

int Media::bandwidth_max_value() {
    return bandwidth_max_value_;
}

bool Media::HasPayloadType(int pt) const {
    return rtp_map_.find(pt) != rtp_map_.end();
}

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
        auto pt = RTPMap::ParsePayloadType(value);
        auto it = rtp_map_.find(pt);
        if (it == rtp_map_.end()) {
            it = rtp_map_.insert(std::make_pair(pt, RTPMap(value))).first;
        }else {
            it->second.SetMLine(value);
        }
        return true;
    }
    // eg: a=rtcp-fb:101 nack pli
    // eg: a=rtcp-fb:101 goog-remb
    else if (key == "rtcp-fb") {
        size_t sp = value.find(' ');
        int pt = utils::string::to_integer<int>(value.substr(0, sp));
        auto it = rtp_map_.find(pt);
        if (it == rtp_map_.end()) {
            it = rtp_map_.insert(std::make_pair(pt, RTPMap())).first;
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
            it = rtp_map_.insert(std::make_pair(pt, RTPMap())).first;
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
        if (!HasSSRC(ssrc)) {
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

// RTPMap
Media::RTPMap::RTPMap(std::string_view mline) {
    SetMLine(mline);
}

// a=rtpmap:102 H264/90000
// mline = 102 H264/90000
void Media::RTPMap::SetMLine(std::string_view mline) {
    size_t p = mline.find(' ');
    if (p == std::string::npos) {
        throw std::invalid_argument("Invalid m-line");
    }

    this->pt = utils::string::to_integer<int>(mline.substr(0, p));

    std::string_view line = mline.substr(p + 1);
    // find separator line
    size_t spl = line.find('/');
    if (spl == std::string::npos) {
        throw std::invalid_argument("Invalid m-line");
    }

    this->format = line.substr(0, spl);

    line = line.substr(spl + 1);
    spl = line.find('/');

    if (spl == std::string::npos) {
        spl = line.find(' ');
    }

    if (spl == std::string::npos) {
        this->clock_rate = utils::string::to_integer<int>(line);
    }else {
        this->clock_rate = utils::string::to_integer<int>(line.substr(0, spl));
        this->codec_params = line.substr(spl + 1);
    }

}

int Media::RTPMap::ParsePayloadType(std::string_view line) {
    size_t p = line.find(' ');
    return utils::string::to_integer<int>(line.substr(0, p));
}

void Media::RTPMap::AddFeedback(const std::string line) {
    this->rtcp_feedbacks.emplace_back(std::move(line));
}

void Media::RTPMap::RemoveFeedback(const std::string& line) {
    for (auto it = this->rtcp_feedbacks.begin(); it != this->rtcp_feedbacks.end(); ++it) {
        if (it->find(line) != std::string::npos) {
            it = this->rtcp_feedbacks.erase(it);
        }
    }
}

void Media::AddRTPMap(const RTPMap& map) {
    rtp_map_.emplace(map.pt, map);
}

} // namespace sdp
} // namespace naivert 