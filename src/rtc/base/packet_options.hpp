#ifndef _RTC_BASE_PACKET_OPTIONS_H_
#define _RTC_BASE_PACKET_OPTIONS_H_

#include "base/defines.hpp"
#include "rtc/base/dscp.hpp"

namespace naivertc {

enum class PacketKind {
    BINARY,
    TEXT,
    AUDIO,
    VIDEO
};

// The structure holds meta infomation for the packet 
// which is about to send over network
struct RTC_CPP_EXPORT PacketOptions {
    PacketOptions(DSCP dscp = DSCP::DSCP_DF, PacketKind kind = PacketKind::BINARY) : dscp(dscp), kind(kind) {}
    ~PacketOptions() = default;
    DSCP dscp;
    PacketKind kind;
};
    
} // namespace naivertc


#endif