#include "rtc/sdp/sdp_media_entry.hpp"
#include "rtc/sdp/sdp_utils.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

#include <sstream>

namespace naivertc {
namespace sdp {

MediaEntry::MediaEntry(const std::string& mline, const std::string mid) 
    : mid_(std::move(mid)) {
    unsigned int port;
    std::istringstream ss(mline);
    ss >> type_string_ >> port >> description_;
    type_ = type_string_to_type(type_string_);
}

MediaEntry::Type MediaEntry::type_string_to_type(std::string_view type_string) const {
    if (type_string == "application" || type_string == "APPLICATION") {
        return Type::APPLICATION;
    }else if (type_string == "audio" || type_string == "AUDIO") {
        return Type::AUDIO;
    }else if (type_string == "video" || type_string == "VIDEO") {
        return Type::VIDEO;
    }else {
        throw std::invalid_argument("Unknown entry type" + std::string(type_string));
    }
}

std::string MediaEntry::GenerateSDP(std::string_view eol, Role role) const {
    std::ostringstream oss;
    std::string sp = " ";

    // 0.0.0.0：表示你要用来接收或者发送音频使用的IP地址，webrtc使用ice传输，不使用这个地址
    // 9：代表音频使用端口9来传输
    const auto addr = "0.0.0.0";
    const auto port = "9";
    oss << "m=" << type_string() << sp << port << sp << description() << eol;
    // connection line: c=<nettype> <addrtype> <connection-address>
    // nettype: network type, eg: IN represents Internet
    // addrtype: address type, eg: IPv4, IPv6
    // connection-address
    oss << "c=IN" << sp << "IP4" << sp <<  addr << eol;
    if (type_ != Type::APPLICATION) {
        oss << "a=rtcp:" << port << sp << "IN" << sp << "IP4" << sp << addr;
    }
    // ICE and DTLS lines
    oss << Entry::GenerateSDP(eol, role);
    
    // Media-level attributes
    oss << GenerateSDPLines(eol);

    return oss.str();
}

std::string MediaEntry::GenerateSDPLines(std::string_view eol) const {
    std::ostringstream oss;
    // 与属性a=group:BUNDLE配合使用，表示多个media使用同一个端口
    // 'bundle-only' which can be used to request that specified meida
    // is only used if kept within a BUNDLE group.
    // See https://datatracker.ietf.org/doc/html/draft-ietf-mmusic-sdp-bundle-negotiation-38#section-6
    oss << "a=bundle-only" << eol;
    oss << "a=mid:" << mid_ << eol;

    // Extra attributes
    for (const auto& attr : attributes_) {
        // extmap：表示rtp报头拓展的映射，可能有多个，eg: a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
        // rtcp-resize(rtcp reduced size), 表示rtcp包是使用固定算法缩小过的
        // TODO: Except "extmap" and "rtcp-rsize", but why?
        // 是不是表示当前版本暂不支持这个两个属性
        if (attr.find("extmap") == std::string::npos && attr.find("rtcp-rsize") == std::string::npos) {
            oss << "a=" << attr << eol;
        }
    }

    return oss.str();
}

bool MediaEntry::ParseSDPLine(std::string_view line) {
    if (utils::string::match_prefix(line, "a=")) {
        std::string_view attr = line.substr(2);
        auto [key, value] = utils::string::parse_pair(attr);
        return ParseSDPAttributeField(key, value);    
    // Connection
    }else if (utils::string::match_prefix(line, "c=")) {
        // Ignore
        return true;
    }
    return false;
}  

bool MediaEntry::ParseSDPAttributeField(std::string_view key, std::string_view value) {
    if (key == "mid") {
        mid_ = value;
        return true;
    }else if (value == "bundle-only") {
        // Added already
        return true;
    }else if (key == "setup") {
        // Parsed at session-level
        return false;
    }else {
        return Entry::ParseSDPAttributeField(key, value);
    }
}

} // namespace sdp
} // namespace naivertc