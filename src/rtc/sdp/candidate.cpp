#include "rtc/sdp/candidate.hpp"
#include "common/utils_string.hpp"
#include "common/utils_network.hpp"

#include <plog/Log.h>

#include <unordered_map>
#include <array>
#include <sstream>

namespace naivertc {
namespace sdp {

Candidate::Candidate() :
    foundation_("none"),
    component_id_(0),
    priority_(0),
    transport_type_(TransportType::UNKNOWN),
    hostname_("0.0.0.0"),
    server_port_("9"),
    type_(Type::UNKNOWN),
    family_(Family::UNRESOVLED) {}

Candidate::Candidate(std::string candidate) : Candidate() {
    if (!candidate.empty()) {
        Parse(std::move(candidate));
    }
}

Candidate::Candidate(std::string candidate, const std::string mid) : Candidate() {
    if (!candidate.empty()) {
        Parse(std::move(candidate));
    }
    if (!mid.empty()) {
        mid_.emplace(std::move(mid));
    }
}

Candidate::~Candidate() {}

// Accessor
const std::string Candidate::foundation() const {
    return foundation_;
}

uint32_t Candidate::component_id() const {
    return component_id_;
}

Candidate::Type Candidate::type() const {
    return type_;
}

Candidate::TransportType Candidate::transport_type() const {
    return transport_type_;
}

uint32_t Candidate::priority() const {
    return priority_;
}

const std::string Candidate::hostname() const {
    return hostname_;
}

const std::string Candidate::server_port() const {
    return server_port_;
}

const std::string Candidate::mid() const {
    return mid_.value_or("0");
}

void Candidate::HintMid(std::string mid) {
    if(!mid.empty())
        mid_.emplace(std::move(mid));
}

std::string Candidate::sdp_line() const {
    std::ostringstream line;
    line << "a=" << std::string(*this);
    return line.str();
}

Candidate::operator std::string() const {
    const char sp{' '};
    std::ostringstream oss;
    oss << "candidate:";
    oss << foundation_ << sp << component_id_ << sp << transport_type_str_ << sp << priority_ << sp;
    if (isResolved()) {
        oss << address_ << sp << port_;
    }else {
        oss << hostname_ << sp << server_port_;
    }

    oss << sp << "typ" << sp << type_str_;

    if (!various_tail_.empty()) {
        oss << sp << various_tail_;
    }

    return oss.str();
}

bool Candidate::isResolved() const {
    return family_ != Family::UNRESOVLED;
}

Candidate::Family Candidate::family() const {
    return family_;
}

std::optional<std::string> Candidate::address() const {
    return isResolved() ? std::make_optional(address_) : std::nullopt;
}

std::optional<uint16_t> Candidate::port() const {
    return isResolved() ? std::make_optional(port_) : std::nullopt;
}

bool Candidate::operator==(const Candidate& other) const {
    return (foundation_ == other.foundation_ && 
            server_port_ == other.server_port_ &&
            hostname_ == other.hostname_
            );
}

bool Candidate::operator!=(const Candidate& other) const {
    return foundation_ != other.foundation_;
}

// Public Methods
// 通过域名解析ip地址，一个域名可对应多个ip地址
bool Candidate::Resolve(ResolveMode mode) {
    PLOG_VERBOSE << "Resolving candidate (mode="
                 << (mode == ResolveMode::SIMPLE ? "simple" : "lookup") 
                 << "): " << hostname_ << ":" << server_port_;

    utils::network::ProtocolType protocol_type = utils::network::ProtocolType::UNKNOWN;
    if (transport_type_ == TransportType::UDP) {
        protocol_type = utils::network::ProtocolType::UDP;
    }else if (transport_type_ != TransportType::UNKNOWN) {
        protocol_type = utils::network::ProtocolType::TCP;
    }

    auto resolve_result = utils::network::UnspecfiedResolve(hostname_, server_port_, protocol_type, mode == ResolveMode::SIMPLE);

    if (resolve_result.has_value()) {
        family_ = resolve_result.value().is_ipv6 ? Family::IP_V6 : Family::IP_V4;
        address_ = std::move(resolve_result.value().address);
        port_ = resolve_result.value().port;
        return true;
    }else {
        return false;
    }
}

// Private Methods
void Candidate::Parse(std::string candidate) {
    using candidate_type_map_t = std::unordered_map<std::string, Type>;
    using tcp_transport_type_map_t = std::unordered_map<std::string, TransportType>;

    static const candidate_type_map_t candidate_type_map = {
        {"host", Type::HOST},
        {"srflx", Type::SERVER_REFLEXIVE},
        {"prflx", Type::PEER_REFLEXIVE},
        {"relay", Type::RELAYED}
    };

    static const tcp_transport_type_map_t tcp_trans_type_map = {
        {"active", TransportType::TCP_ACTIVE},
        {"passive", TransportType::TCP_PASSIVE},
        {"so", TransportType::TCP_S_O}
    };

    const std::array<std::string, 2> prefixes = {"a=", "candidate:"};
    for (std::string_view prefix : prefixes) {
        if (utils::string::match_prefix(candidate, prefix)) {
            candidate.erase(0, prefix.size());
        }
    }
    
    PLOG_VERBOSE << "Parsing candidate: " << candidate;

    std::istringstream iss(candidate);
    // “a=candidate:1 1 UDP 9654321 212.223.223.223 12345 typ srflx raddr 10.216.33.9 rport 54321 generation 0 ufrag CE1b network-id 1 network-cost 10”
    // foundation = 1
    // component id = 1 (1: RTP, 2: RTCP)
    // transport yype = UDP 
    // priority = 9654321
    // host name（公网ip或域名）= 212.223.223.223
    // server port (公网端口) = 12345
    // typ = indicate the next one is candidate type 
    // candidate type = srflx
    // base host = 10.216.33.9
    // base port = 54321
    // generation =0
    // ufrag (the username fragment that uniquely identifies a single ICE interaction session) = CE1b, 
    // see https://developer.mozilla.org/en-US/docs/Web/API/RTCIceCandidate/usernameFragment
    // network-id = 1 and network-cost = 10, see https://datatracker.ietf.org/doc/html/draft-thatcher-ice-network-cost-00#section-5
    std::string type_indicator;
    // 使用istringstream对格式化的字符串（以空格隔开）进行重定向
    if (!(iss >> foundation_ >> component_id_ >> transport_type_str_ >> priority_ 
        >> hostname_ >> server_port_ >> type_indicator >> type_str_) && type_indicator == "typ") {
        throw std::invalid_argument("Invalid candidate format");
    }

    // Retrieve candidate enum type
    auto it = candidate_type_map.find(type_str_);
    if (it != candidate_type_map.end()) {
        type_ = it->second;
    }else {
        type_ = Type::UNKNOWN;
    }

    // Keep a copy of left parameters after 'type', like network-id, network-cost
    // see https://datatracker.ietf.org/doc/html/draft-thatcher-ice-network-cost-00
    std::getline(iss, various_tail_);
    utils::string::trim_begin(various_tail_);
    utils::string::trim_end(various_tail_);

    if (transport_type_str_ == "UDP" || transport_type_str_ == "udp") {
        transport_type_ = TransportType::UDP;
    // 如果传输协议是TCP，则进一步检测具体的TCP映射类型
    }else if (transport_type_str_ == "TCP" || transport_type_str_ == "tcp") {
        std::string tcp_type_indicator, tcp_type_string;
        if (iss >> tcp_type_indicator >> tcp_type_string && tcp_type_indicator == "tcptype") {
            auto it = tcp_trans_type_map.find(tcp_type_string);
            if (it != tcp_trans_type_map.end()) {
                transport_type_ = it->second;
            }else {
                transport_type_ = TransportType::TCP_UNKNOWN;
            }
        }else {
            transport_type_ = TransportType::TCP_UNKNOWN;
        }
    }else {
        transport_type_ = TransportType::UNKNOWN;
    }
}

} // namespace sdp
} // namespace naivertc

std::ostream &operator<<(std::ostream &out, const naivertc::sdp::Candidate& candidate) {
    return out << std::string(candidate);
}

std::ostream &operator<<(std::ostream &out, const naivertc::sdp::Candidate::Type type) {
    switch(type) {
    case naivertc::sdp::Candidate::Type::HOST:
        return out << "host";
    case naivertc::sdp::Candidate::Type::PEER_REFLEXIVE:
        return out << "prflx";
    case naivertc::sdp::Candidate::Type::SERVER_REFLEXIVE:
        return out << "srflx";
    case naivertc::sdp::Candidate::Type::RELAYED:
        return out << "relay";
    default:
        return out << "unknown";
    }
}

std::ostream &operator<<(std::ostream &out, const naivertc::sdp::Candidate::TransportType type) {
    switch (type)
    {
    case naivertc::sdp::Candidate::TransportType::UDP:
        return out << "UDP";
    case naivertc::sdp::Candidate::TransportType::TCP_ACTIVE:
        return out << "TCP_ACTIVE";
    case naivertc::sdp::Candidate::TransportType::TCP_PASSIVE:
        return out << "TCP_PASSIVE";
    case naivertc::sdp::Candidate::TransportType::TCP_S_O:
        return out << "TCP_S_O";
    case naivertc::sdp::Candidate::TransportType::TCP_UNKNOWN:
        return out << "TCP_UNKNOWN";
    default:
        return out << "unknown";
    }
}

