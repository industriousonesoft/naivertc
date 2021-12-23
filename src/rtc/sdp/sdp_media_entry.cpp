#include "rtc/sdp/sdp_media_entry.hpp"
#include "rtc/sdp/sdp_utils.hpp"
#include "common/utils_string.hpp"

#include <plog/Log.h>

#include <sstream>

namespace naivertc {
namespace sdp {

MediaEntry MediaEntry::Parse(const std::string& mline, std::string mid) {
    std::istringstream iss(mline);
    int port = 0;
    std::string kind_string;
    std::string protocols;
    iss >> kind_string >> port >> protocols;
    return MediaEntry(ToKind(kind_string), std::move(mid), protocols);
}

MediaEntry::MediaEntry(Kind kind, 
                       std::string mid, 
                       std::string protocols) 
    : kind_(kind), 
      mid_(std::move(mid)),
      protocols_(protocols) {}

std::string MediaEntry::GenerateSDP(const std::string eol, Role role) const {
    std::ostringstream oss;
    std::string sp = " ";

    // m=<media> <port> <proto> <fmt>
    // See https://datatracker.ietf.org/doc/html/rfc4566#section-5.14
    // 0.0.0.0：表示你要用来接收或者发送音频使用的IP地址，webrtc使用ice传输，不使用这个地址
    // 9：代表音频使用端口9来传输
    const auto addr = "0.0.0.0";
    const auto port = "9";
    oss << "m=" << kind_ << sp << port << sp << protocols_ << sp << FormatDescription() << eol;
    // connection line: c=<nettype> <addrtype> <connection-address>
    // nettype: network type, eg: IN represents Internet
    // addrtype: address type, eg: IPv4, IPv6
    // connection-address
    oss << "c=IN" << sp << "IP4" << sp <<  addr << eol;
    if (kind_ != Kind::APPLICATION) {
        // a=rtcp:<port> <nettype> <addrtype> <connection-address>
        // See https://tools.ietf.org/id/draft-ietf-mmusic-sdp4nat-00.txt
        oss << "a=rtcp:" << port << sp << "IN" << sp << "IP4" << sp << addr << eol;
    }
    // ICE and DTLS lines
    oss << Entry::GenerateSDP(eol, role);
    
    // Media-level attributes
    oss << GenerateSDPLines(eol);

    return oss.str();
}

std::string MediaEntry::GenerateSDPLines(const std::string eol) const {
    std::ostringstream oss;
    // 与属性a=group:BUNDLE配合使用，表示多个media使用同一个端口
    // 'bundle-only' which can be used to request that specified meida
    // is only used if kept within a BUNDLE group.
    // See https://datatracker.ietf.org/doc/html/draft-ietf-mmusic-sdp-bundle-negotiation-38#section-6
    oss << "a=bundle-only" << eol;
    oss << "a=mid:" << mid_ << eol;
    
    return oss.str();
}

std::string MediaEntry::FormatDescription() const {
    return "";
}

bool MediaEntry::ParseSDPLine(std::string_view line) {
    if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);
        auto [key, value] = utils::string::parse_pair(attr);
        return ParseSDPAttributeField(key, value);    
    // Connection
    } else if (utils::string::match_prefix(line, "c=")) {
        // Ignore
        return true;
    }
    return false;
}  

bool MediaEntry::ParseSDPAttributeField(std::string_view key, std::string_view value) {
    if (key == "mid") {
        mid_ = value;
        return true;
    } else if (value == "bundle-only") {
        // Added already
        return true;
    } else {
        return Entry::ParseSDPAttributeField(key, value);
    }
}

MediaEntry::Kind MediaEntry::ToKind(const std::string_view kind_string) {
    if (kind_string == "application" || kind_string == "APPLICATION") {
        return Kind::APPLICATION;
    } else if (kind_string == "audio" || kind_string == "AUDIO") {
        return Kind::AUDIO;
    } else if (kind_string == "video" || kind_string == "VIDEO") {
        return Kind::VIDEO;
    } else {
        throw std::invalid_argument("Unknown entry kind" + std::string(kind_string));
    }
}

// Ostream <<
std::ostream& operator<<(std::ostream& out, MediaEntry::Kind kind) {
    using Kind = MediaEntry::Kind;
    switch (kind) {
    case Kind::AUDIO:
        out << "audio";
        break;
    case Kind::VIDEO:
        out << "video";
        break;
    case Kind::APPLICATION:
        out << "application";
        break;
    default:
        break;
    }
    return out;
}

} // namespace sdp
} // namespace naivertc