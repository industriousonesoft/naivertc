#include "pc/candidate.hpp"

#include <plog/Log.h>

#include <unordered_map>
#include <array>
#include <sstream>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

const size_t MAX_NUMERICNODE_LEN = 48; // Max IPv6 string representation length
const size_t MAX_NUMERICSERV_LEN = 6;  // Max port string representation length

namespace {
    inline bool MatchCandidatePrefix(const std::string &str, const std::string &prefix) {
        return str.size() >= prefix.size() && std::mismatch(prefix.begin(), prefix.end(), str.begin()).first == prefix.end();
    }

    inline void TrimBegin(std::string &str) {
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](char c){
            return !std::isspace(c);
        }));
    }

    inline void TrimEnd(std::string &str) {
        // reverse_iterator.base() -> iterator
        str.erase(std::find_if(str.rbegin(), str.rend(), [](char c){
            return !std::isspace(c);
        }).base(), str.end());
    }
}

namespace naivertc {

Candidate::Candidate() :
    foundation_("none"),
    component_id_(0),
    priority_(0),
    transport_type_(TransportType::UNKNOWN),
    host_name_("0.0.0.0"),
    service_("9"),
    type_(Type::UNKNOWN) {}

Candidate::Candidate(std::string candidate) : Candidate() {
    if (!candidate.empty()) {
        Parse(candidate);
    }
}

Candidate::Candidate(std::string candidate, std::string mid) : Candidate() {
    if (!candidate.empty()) {
        Parse(candidate);
    }
    if (!mid.empty()) {
        mid_.emplace(mid);
    }
}

Candidate::~Candidate() {}

// Accessor
Candidate::Type Candidate::GetType() const {
    return type_;
}

Candidate::TransportType Candidate::GetTransportType() const {
    return transport_type_;
}

uint32_t Candidate::GetPriority() const {
    return priority_;
}

std::string Candidate::ResolvedCandidate() const {
    const char sp{' '};
    std::ostringstream oss;
    oss << "candidate:";
    oss << foundation_ << sp << component_id_ << sp << transport_type_str_ << sp << priority_ << sp;
    if (isResolved()) {
        oss << address_ << sp << port_;
    }else {
        oss << host_name_ << sp << service_;
    }

    oss << sp << "typ" << sp << type_str_;

    if (!various_tail_.empty()) {
        oss << sp << various_tail_;
    }

    return oss.str();
}

std::string Candidate::GetMid() const {
    return mid_.value_or("0");
}

bool Candidate::isResolved() const {
    return family_ != Family::UNRESOVLED;
}

Candidate::Family Candidate::GetFamily() const {
    return family_;
}

std::optional<std::string> Candidate::Address() const {
    return isResolved() ? std::make_optional(address_) : std::nullopt;
}

std::optional<uint16_t> Candidate::Port() const {
    return isResolved() ? std::make_optional(port_) : std::nullopt;
}

std::string Candidate::SDPLine() const {
    std::ostringstream line;
    line << "a=" << ResolvedCandidate();
    return line.str();
}

bool Candidate::operator==(const Candidate& other) const {
    return (foundation_ == other.foundation_ && 
            service_ == other.service_ &&
            host_name_ == other.host_name_
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
                 << "): " << host_name_ << ":" << service_;

    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_ADDRCONFIG;
    if (transport_type_ == TransportType::UDP) {
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
    }else if (transport_type_ != TransportType::UNKNOWN) {
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
    }

    if (mode == ResolveMode::SIMPLE) {
        hints.ai_flags |= AI_NUMERICHOST;
    }

    struct addrinfo *result = nullptr;
    if (getaddrinfo(host_name_.c_str(), service_.c_str(), &hints, &result) == 0) {
        for (auto p = result; p; p = p ->ai_next) {
            char nodebuffer[MAX_NUMERICNODE_LEN];
            char servbuffer[MAX_NUMERICSERV_LEN];
            if (getnameinfo(p->ai_addr, socklen_t(p->ai_addrlen), nodebuffer,
				                MAX_NUMERICNODE_LEN, servbuffer, MAX_NUMERICSERV_LEN,
				                NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
                try {
                    port_ = uint16_t(std::stoul(servbuffer));
                } catch (...) {
                    return false;
                }
                address_ = nodebuffer;
                family_ = p->ai_family == AF_INET6 ? Family::IP_V6 : Family::IP_V4;
                PLOG_VERBOSE << "Resolved candidate public ip: " << address_ << ":" << port_;
                break;
            }
        }
        freeaddrinfo(result);
    }

    return family_ != Family::UNRESOVLED;
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
    for (std::string prefix : prefixes) {
        if (MatchCandidatePrefix(candidate, prefix)) {
            candidate.erase(0, prefix.size());
        }
    }
    
    PLOG_VERBOSE << "Parsing candidate: " << candidate;

    std::istringstream iss(candidate);
    // “a=candidate:1 1 UDP 9654321 212.223.223.223 12345 typ srflx raddr 10.216.33.9 rport 54321”
    // foundation = 1
    // component id = 1 (1: RTP, 2: RTCP)
    // transport yype = UDP 
    // priority = 9654321
    // host name（公网ip或域名）= 212.223.223.223
    // service (公网端口) = 12345
    // typ = indicate the next one is candidate type 
    // candidate type = srflx
    // base host = 10.216.33.9
    // base port = 54321”
    std::string type_indicator;
    // 使用istringstream对格式化的字符串（以空格隔开）进行重定向
    if (!(iss >> foundation_ >> component_id_ >> transport_type_str_ >> priority_ 
        >> host_name_ >> service_ >> type_indicator >> type_str_) && type_indicator == "typ") {
        throw std::invalid_argument("Invalid candidate format");
    }

    // Retrieve candidate enum type
    auto it = candidate_type_map.find(type_str_);
    if (it != candidate_type_map.end()) {
        type_ = it->second;
    }else {
        type_ = Type::UNKNOWN;
    }

    // Keep a copy of substring after type
    std::getline(iss, various_tail_);
    TrimBegin(various_tail_);
    TrimEnd(various_tail_);

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

} // namespace naive rtc

std::ostream &operator<<(std::ostream &out, const naivertc::Candidate& candidate) {
    return out << candidate.SDPLine();
}

std::ostream &operator<<(std::ostream &out, const naivertc::Candidate::Type type) {
    switch(type) {
    case naivertc::Candidate::Type::HOST:
        return out << "host";
    case naivertc::Candidate::Type::PEER_REFLEXIVE:
        return out << "prflx";
    case naivertc::Candidate::Type::SERVER_REFLEXIVE:
        return out << "srflx";
    case naivertc::Candidate::Type::RELAYED:
        return out << "relay";
    default:
        return out << "unknown";
    }
}

std::ostream &operator<<(std::ostream &out, const naivertc::Candidate::TransportType type) {
    switch (type)
    {
    case naivertc::Candidate::TransportType::UDP:
        return out << "UDP";
    case naivertc::Candidate::TransportType::TCP_ACTIVE:
        return out << "TCP_ACTIVE";
    case naivertc::Candidate::TransportType::TCP_PASSIVE:
        return out << "TCP_PASSIVE";
    case naivertc::Candidate::TransportType::TCP_S_O:
        return out << "TCP_S_O";
    case naivertc::Candidate::TransportType::TCP_UNKNOWN:
        return out << "TCP_UNKNOWN";
    default:
        return out << "unknown";
    }
}

