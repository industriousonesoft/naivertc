#ifndef _RTC_BASE_PACKET_OPTIONS_H_
#define _RTC_BASE_PACKET_OPTIONS_H_

#include "base/defines.hpp"
#include "rtc/base/dscp.hpp"

namespace naivertc {
// The structure holds meta infomation for the packet 
// which is about to send over network
struct RTC_CPP_EXPORT PacketOptions {
    PacketOptions() = default;
    PacketOptions(DSCP dscp) : dscp(dscp) {}
    PacketOptions(const PacketOptions&) = default;
    PacketOptions(PacketOptions&&) = default;
    ~PacketOptions() = default;
    
    DSCP dscp = DSCP_DF;
};
    
} // namespace naivertc


#endif