#include "pc/sdp_entry.hpp"

#include <sstream>

namespace naivertc {

SDPEntry::SDPEntry(const std::string& mline, std::string mid, sdp::Direction direction) 
    : mid_(std::move(mid)), direction_(direction) {
    unsigned int port;
    std::istringstream ss(mline);
    ss >> type_ >> port >> description_;
}

void SDPEntry::set_direction(sdp::Direction direction) {
    direction_ = direction;
}

std::string SDPEntry::GenerateSDP(std::string_view eol, std::string addr, std::string_view port) const {
    std::ostringstream sdp;
    sdp << "m=" << type() << ' ' << port << ' ' << description() << eol;
    sdp << "c=IN " << addr << eol;
    sdp << GenerateSDPLines(eol);

    return sdp.str();
}

std::string SDPEntry::GenerateSDPLines(std::string_view eol) const {
    std::ostringstream sdp;
    sdp << "a=bundle-only" << eol;
    sdp << "a=mid:" << mid_ << eol;

    switch(direction_) {
    case sdp::Direction::SEND_ONLY: 
        sdp << "a=sendonly" << eol;
        break;
    case sdp::Direction::RECV_ONLY: 
        sdp << "a=recvonly" << eol;
        break;
    case sdp::Direction::SEND_RECV: 
        sdp << "a=sendrecv" << eol;
        break;
    case sdp::Direction::INACTIVE: 
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

}