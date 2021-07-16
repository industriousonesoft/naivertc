#include "rtc/sdp/sdp_entry.hpp"
#include "rtc/sdp/sdp_utils.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

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

std::string Entry::GenerateSDP(std::string_view eol, std::string_view addr, std::string_view port) const {
    std::ostringstream sdp;
    std::string sp = " ";
    sdp << "m=" << type_string() << sp << port << sp << description() << eol;
    // connection line: c=<nettype> <addrtype> <connection-address>
    // nettype: network type, eg: IN represents Internet
    // addrtype: address type, eg: IPv4, IPv6
    // connection-address
    sdp << "c=IN" << sp << "IP4" << sp <<  addr << eol;
    if (type_ != sdp::Entry::Type::APPLICATION) {
        sdp << "a=rtcp:" << port << sp << "IN" << sp << "IP4" << sp << addr;
    }
    sdp << GenerateSDPLines(eol);

    return sdp.str();
}

std::string Entry::GenerateSDPLines(std::string_view eol) const {
    std::ostringstream sdp;
    // 与属性a=group:BUNDLE配合使用，表示多个media使用同一个端口
    // 'bundle-only' which can be used to request that specified meida
    // is only used if kept within a BUNDLE group.
    // See https://datatracker.ietf.org/doc/html/draft-ietf-mmusic-sdp-bundle-negotiation-38#section-6
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
        }else if (key == "setup") {
            if (value == "active") {
                role_.emplace(Role::ACTIVE);
            }else if (value == "passive") {
                role_.emplace(Role::PASSIVE);
            }else {
                role_.emplace(Role::ACT_PASS);
            }
        }else if (key == "fingerprint") {
            auto fingerprint = ParseFingerprintAttribute(value);
            if (fingerprint.has_value()) {
                set_fingerprint(std::move(fingerprint.value()));
            }else {
                PLOG_WARNING << "Failed to parse fingerprint format: " << value;
            }
        }else if (key == "ice-ufrag") {
            ice_ufrag_.emplace(std::move(value));
        }else if (key == "ice-pwd") {
            ice_pwd_.emplace(std::move(value));
        }else if (key == "candidate") {
            // TODO：add candidate from sdp
        }else if (key == "end-of-candidate") {
            // TODO：add candidate from sdp
        }else {
            attributes_.emplace_back(std::move(attr));
        }
    }
}  

void Entry::set_fingerprint(std::string fingerprint) {

    if (!IsSHA256Fingerprint(fingerprint)) {
        throw std::invalid_argument("Invalid SHA265 fingerprint: " + fingerprint);
    }

    // make sure All the chars in finger print is uppercase.
    std::transform(fingerprint.begin(), fingerprint.end(), fingerprint.begin(), [](char c) {
        return char(std::toupper(c));
    });

    fingerprint_.emplace(std::move(fingerprint));
}

} // namespace sdp
} // namespace naivertc