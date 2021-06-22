#ifndef _PC_CANDIDATE_H_
#define _PC_CANDIDATE_H_

#include "common/defines.hpp"

#include <string>
#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT Candidate {
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

    std::string Foundation() const;
    uint32_t ComponentId() const;
    Type GetType() const;
    TransportType GetTransportType() const;
    uint32_t Priority() const;
    std::string ResolvedCandidate() const;
    std::string GetMid() const;
    std::string HostName() const;
    std::string Service() const;

    bool isResolved() const;
    Family GetFamily() const;
    std::optional<std::string> Address() const;
    std::optional<uint16_t> Port() const;
 
    bool Resolve(ResolveMode mode = ResolveMode::SIMPLE);

    std::string SDPLine() const;

    bool operator==(const Candidate& other) const;
    bool operator!=(const Candidate& other) const;

private: 
    void Parse(std::string candidate);

private:
    
    Family family_;
    std::string address_;
    uint16_t port_;
    
    std::string foundation_;
    uint32_t component_id_;
    uint32_t priority_;
    TransportType transport_type_;
    std::string transport_type_str_;
    std::string host_name_;
    std::string service_;
    Type type_;
    std::string type_str_;
    std::string various_tail_;

    std::optional<std::string> mid_;
    
};

}

#endif