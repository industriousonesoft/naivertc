#include "pc/sdp/sdp_entry.hpp"
#include "common/utils.hpp"

#include <sstream>

namespace naivertc {
namespace sdp {

// Entry
Entry::Entry(const std::string& mline, std::string mid, Direction direction) 
    : mid_(std::move(mid)), direction_(direction) {
    unsigned int port;
    std::istringstream ss(mline);
    ss >> type_string_ >> port >> description_;
    type_ = type_string_to_type(type_string_);
}

Entry::Type Entry::type_string_to_type(std::string type_string) const {
    if (type_string == "application" || type_string == "APPLICATION") {
        return Type::APPLICATION;
    }else if (type_string == "audio" || type_string == "AUDIO") {
        return Type::AUDIO;
    }else if (type_string == "video" || type_string == "VIDEO") {
        return Type::VIDEO;
    }else {
        throw std::invalid_argument("Unknown entry type: " + type_string);
    }
}

void Entry::set_direction(Direction direction) {
    direction_ = direction;
}

std::string Entry::GenerateSDP(std::string_view eol, std::string addr, std::string_view port) const {
    std::ostringstream sdp;
    sdp << "m=" << type_string() << ' ' << port << ' ' << description() << eol;
    sdp << "c=IN " << addr << eol;
    sdp << GenerateSDPLines(eol);

    return sdp.str();
}

std::string Entry::GenerateSDPLines(std::string_view eol) const {
    std::ostringstream sdp;
    sdp << "a=bundle-only" << eol;
    sdp << "a=mid:" << mid_ << eol;

    switch(direction_) {
    case Direction::SEND_ONLY: 
        sdp << "a=sendonly" << eol;
        break;
    case Direction::RECV_ONLY: 
        sdp << "a=recvonly" << eol;
        break;
    case Direction::SEND_RECV: 
        sdp << "a=sendrecv" << eol;
        break;
    case Direction::INACTIVE: 
        sdp << "a=inactive" << eol;
        break;
    default:
        break;
    }

    for (const auto& attr : attributes_) {
        // extmap：表示rtp报头拓展的映射，可能有多个，eg: a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
        // rtcp-resize(rtcp reduced size), 表示rtcp包是使用固定算法缩小过的
        // TODO: Except "extmap" and "rtcp-rsize", but why?
        // 是不是表示当前版本暂不支持这个两个属性
        if (attr.find("extmap") == std::string::npos && attr.find("rtcp-rsize") == std::string::npos) {
            sdp << "a=" << attr << eol;
        }
    }

    return sdp.str();
}

void Entry::ParseSDPLine(std::string_view line) {
    if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);
        auto [key, value] = utils::string::parse_pair(attr);

        if (key == "mid") {
            mid_ = value;
        }else if (attr == "sendonly") {
            direction_ = Direction::SEND_ONLY;
        }else if (attr == "recvonly") {
            direction_ = Direction::RECV_ONLY;
        }else if (attr == "sendrecv") {
            direction_ = Direction::SEND_RECV;
        }else if (attr == "inactive") {
            direction_ = Direction::INACTIVE;
        }else if (attr == "bundle-only") {
            // Added already
        }else {
            attributes_.emplace_back(std::move(attr));
        }
    }
}   

// Application
Application::Application(std::string mid)
    : Entry("application 9 UDP/DTLS/SCTP", std::move(mid), Direction::SEND_RECV) {}

std::string Application::description() const {
    return Entry::description() + " webrtc-datachannel";
}

Application Application::reciprocate() const {
    Application reciprocate(*this);
    reciprocate.max_message_size_.reset();
    return reciprocate;
}

void Application::ParseSDPLine(std::string_view line) {
    if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);
        auto [key, value] = utils::string::parse_pair(attr);

        if (key == "sctp-port") {
            sctp_port_ = utils::string::to_integer<uint16_t>(value);
        }else if (key == "max-message-size") {
            max_message_size_ = utils::string::to_integer<size_t>(value);
        }else {
            Entry::ParseSDPLine(line);
        }
    }else {
        Entry::ParseSDPLine(line);
    }
}

std::string Application::GenerateSDPLines(std::string_view eol) const {
    std::ostringstream sdp;
    sdp << Entry::GenerateSDPLines(eol);

    if (sctp_port_) {
        sdp << "a=sctp-port:" << *sctp_port_ << eol;
    }

    if (max_message_size_) {
        sdp << "a=max-message-size:" << *max_message_size_ << eol;
    }

    return sdp.str();
}


// Media
Media::Media(const std::string& sdp) 
    : Entry(sdp, "", Direction::UNKNOWN) {}
    
Media::Media(const std::string& mline, std::string mid, Direction direction) 
    : Entry(mline, std::move(mid), direction) {}

std::string Media::description() const {
    std::ostringstream desc;
    desc << Entry::description();
    
    for (auto it = rtp_map_.begin(); it != rtp_map_.end(); ++it) {
        desc << ' ' << it->first;
    }

    return desc.str();
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

void Media::ParseSDPLine(std::string_view line) {
    if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);
        auto [key, value] = utils::string::parse_pair(attr);

        // eg: a=rtpmap:101 VP9/90000
        if (key == "rtpmap") {
            auto pt = RTPMap::ParsePayloadType(value);
            auto it = rtp_map_.find(pt);
            if (it == rtp_map_.end()) {
                it = rtp_map_.insert(std::make_pair(pt, RTPMap(value))).first;
            }else {
                it->second.SetMLine(value);
            }
        // eg: a=rtcp-fb:101 nack pli
        // eg: a=rtcp-fb:101 goog-remb
        }else if (key == "rtcp-fb") {
            size_t sp = value.find(' ');
            int pt = utils::string::to_integer<int>(value.substr(0, sp));
            auto it = rtp_map_.find(pt);
            if (it == rtp_map_.end()) {
                it = rtp_map_.insert(std::make_pair(pt, RTPMap())).first;
            }
            it->second.rtcp_feedbacks.emplace_back(value.substr(sp + 1));
        // eg: a=fmtp:107 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
        }else if (key == "fmtp") {
            size_t sp = value.find(' ');
            int pt = utils::string::to_integer<int>(value.substr(0, sp));
            auto it = rtp_map_.find(pt);
            if (it == rtp_map_.end()) {
                it = rtp_map_.insert(std::make_pair(pt, RTPMap())).first;
            }
            it->second.fmt_profiles.emplace_back(value.substr(sp + 1));
        }else if (key == "rtcp-mux") {
            // Added by default
        // eg: a=ssrc:3463951252 cname:sTjtznXLCNH7nbRw
        }else if (key == "ssrc") {
            auto ssrc = utils::string::to_integer<uint32_t>(value);
            if (!HasSSRC(ssrc)) {
                ssrcs_.emplace_back(ssrc);
            }
            auto cname_pos = value.find("cname:");
            if (cname_pos != std::string::npos) {
                auto cname = value.substr(cname_pos + 6);
                cname_map_.emplace(ssrc, cname);
            }
            attributes_.emplace_back(attr);
        }else {
            Entry::ParseSDPLine(line);
        }
    // 'b=AS', is used to negotiate the maximum bandwidth
    // eg: b=AS:80
    }else if (utils::string::match_prefix(line, "b=AS")) {
        bandwidth_max_value_ = utils::string::to_integer<int>(line.substr(line.find(':') + 1));
    }else {
        Entry::ParseSDPLine(line);
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

void Media::RTPMap::AddFeedback(const std::string& line) {
    this->rtcp_feedbacks.emplace_back(line);
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

// Audio
/**
 * UDP/TLS/RTP/SAVPF：指明使用的传输协议，其中SAVPF是由S(secure)、F(feedback)、AVP（RTP A(audio)/V(video) profile, 详解rfc1890）组成
 * 即传输层使用UDP协议，并采用DTLS(UDP + TLS)，在传输层之上使用RTP(RTCP)协议，具体的RTP格式是SAVPF
 * 端口为9（可忽略，端口9为Discard Protocol专用），采用UDP传输加密的RTP包，并使用基于SRTCP的音视频反馈机制来提升传输质量
*/
Audio::Audio(std::string mid, Direction direction) 
    : Media("audio 9 UDP/TLS/RTP/SAVPF", std::move(mid), direction) {}

void Audio::AddAudioCodec(int payload_type, std::string codec, int clock_rate, int channels, std::optional<std::string> profile) {
    RTPMap map(std::to_string(payload_type) + " " + codec + "/" + std::to_string(clock_rate) + "/" + std::to_string(channels));
    if (profile) {
        map.fmt_profiles.emplace_back(*profile);
    }
    AddRTPMap(map);
}

// Video
Video::Video(std::string mid, Direction direction) 
    : Media("video 9 UDP/TLS/RTP/SAVPF", std::move(mid), direction) {}

void Video::AddVideoCodec(int payload_type, std::string codec, std::optional<std::string> profile) {
    RTPMap map(std::to_string(payload_type) + " " + codec + "/90000");
    // TODO: Replace fixed feedback settings with input parameters
    map.AddFeedback("nack");
    map.AddFeedback("nack pli");
    map.AddFeedback("goog-remb");
    if (profile)
        map.fmt_profiles.emplace_back(*profile);

    AddRTPMap(map);
}

} // namespace sdp
} // namespace naivertc