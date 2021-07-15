#ifndef _PC_CANDIDATE_H_
#define _PC_CANDIDATE_H_

#include "base/defines.hpp"

#include <string>
#include <optional>

namespace naivertc {

struct RTC_CPP_EXPORT Candidate {
public: 
    enum class Family {
        UNRESOVLED,
        IP_V4,
        IP_V6
    };

    enum class Type {
        UNKNOWN,
        HOST,
        SERVER_REFLEXIVE,
        PEER_REFLEXIVE,
        RELAYED
    };

    enum class TransportType {
        UNKNOWN,
        UDP,
        // TCP ICE Candidate: https://tools.ietf.org/id/draft-ietf-mmusic-ice-tcp-16.html#rfc.section.3
        /** 
         * When the agents perform address allocations to gather TCP-based candidates, 
         * three types of candidates can be obtained. These are active candidates, passive candidates, 
         * and simultaneous-open (S-O) candidates.
         * An active candidate is one for which the agent will attempt to open an outbound connection, but will not receive incoming connection requests. 
         * A passive candidate is one for which the agent will receive incoming connection attempts, but not attempt a connection. 
         * S-O candidate is one for which the agent will attempt to open a connection simultaneously with its peer.
        */
        TCP_ACTIVE, 
        TCP_PASSIVE,
        TCP_S_O,
        TCP_UNKNOWN
    };

    enum class ResolveMode {
        SIMPLE,
        LOOK_UP
    };

    Candidate();
    Candidate(std::string candidate);
    Candidate(std::string candidate, std::string mid);

    ~Candidate();

    // NOTE: 对于成员变量的Get\Set函数，使用下划线的命名方式，参考webrtc源码
    // 我猜测的原因有两个：
    // 一是避免函数名与自定义的类型命名冲突,
    // 二是根据函数名可识别出是Get\Set函数，以区分于普通函数
    std::string foundation() const;
    uint32_t component_id() const;
    Type type() const;
    TransportType transport_type() const;
    uint32_t priority() const;
    std::string hostname() const;
    std::string server_port() const;
    Family family() const;

    std::string mid() const;
    void HintMid(std::string mid);

    // NOTE: 对于普通的函数使用驼峰命名方式
    bool isResolved() const;
    
    std::optional<std::string> address() const;
    std::optional<uint16_t> port() const;
 
    bool Resolve(ResolveMode mode = ResolveMode::SIMPLE);

    std::string sdp_line() const;
    operator std::string() const;

    bool operator==(const Candidate& other) const;
    bool operator!=(const Candidate& other) const;

private: 
    void Parse(std::string candidate);

private:
    std::string foundation_;
    uint32_t component_id_;
    uint32_t priority_;
    TransportType transport_type_;
    std::string transport_type_str_;
    std::string hostname_;
    std::string server_port_;
    Type type_;
    std::string type_str_;
    std::string various_tail_;

    // TODO: wrap properties blow maybe?
    Family family_;
    std::string address_;
    uint16_t port_;

    std::optional<std::string> mid_{std::nullopt};
    
};

}

#endif