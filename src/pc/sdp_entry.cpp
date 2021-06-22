#include "pc/sdp_entry.hpp"
#include "common/str_utils.hpp"

#include <sstream>

namespace naivertc {
namespace sdp {

Entry::Entry(const std::string& mline, std::string mid, Direction direction) 
    : mid_(std::move(mid)), direction_(direction) {
    unsigned int port;
    std::istringstream ss(mline);
    ss >> type_ >> port >> description_;
}

void Entry::set_direction(Direction direction) {
    direction_ = direction;
}

std::string Entry::GenerateSDP(std::string_view eol, std::string addr, std::string_view port) const {
    std::ostringstream sdp;
    sdp << "m=" << type() << ' ' << port << ' ' << description() << eol;
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
    if (utils::MatchPrefix(line, "a=")) {
        std::string_view attr = line.substr(2);
        auto [key, value] = utils::ParsePair(attr);

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

}
}