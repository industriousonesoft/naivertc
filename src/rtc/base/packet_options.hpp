#ifndef _RTC_BASE_PACKET_OPTIONS_H_
#define _RTC_BASE_PACKET_OPTIONS_H_

#include "base/defines.hpp"
#include "rtc/base/dscp.hpp"

#include <optional>

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
    PacketOptions(PacketKind kind);
    PacketOptions(PacketKind kind, DSCP dscp);
    ~PacketOptions();
    PacketKind kind;
    DSCP dscp;
    
    // Transport sequence number
    std::optional<uint16_t> packet_id;
};
    
} // namespace naivertc


#endif