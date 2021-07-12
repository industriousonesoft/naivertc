#ifndef _PC_SCTP_TRANSPORT_USR_SCTP_SETTINGS_H_
#define _PC_SCTP_TRANSPORT_USR_SCTP_SETTINGS_H_

#include "base/defines.hpp"

#include <optional>
#include <chrono>

namespace naivertc {

struct SctpCustomizedSettings {
    // For the following settings, not set means optimized default
    std::optional<size_t> recvBufferSize;                // in bytes
    std::optional<size_t> sendBufferSize;                // in bytes
    std::optional<size_t> maxChunksOnQueue;              // in chunks
    std::optional<size_t> initialCongestionWindow;       // in MTUs
    std::optional<size_t> maxBurst;                      // in MTUs
    std::optional<unsigned int> congestionControlModule; // 0: RFC2581, 1: HSTCP, 2: H-TCP, 3: RTCC
    std::optional<std::chrono::milliseconds> delayedSackTime;
    std::optional<std::chrono::milliseconds> minRetransmitTimeout;
    std::optional<std::chrono::milliseconds> maxRetransmitTimeout;
    std::optional<std::chrono::milliseconds> initialRetransmitTimeout;
    std::optional<unsigned int> maxRetransmitAttempts;
    std::optional<std::chrono::milliseconds> heartbeatInterval;
};
    
} // namespace naivertc


#endif