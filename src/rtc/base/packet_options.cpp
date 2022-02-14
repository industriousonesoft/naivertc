#include "rtc/base/packet_options.hpp"

namespace naivertc {
namespace {

DSCP DscpForKind(PacketKind kind) {
    // Set recommended medium-priority DSCP value
    // See https://datatracker.ietf.org/doc/html/draft-ietf-tsvwg-rtcweb-qos-18
    if (kind == PacketKind::AUDIO) {
        // EF: Expedited Forwarding
        return DSCP::DSCP_EF; 
    } else if (kind == PacketKind::VIDEO) {
        // AF42: Assured Forwarding class 4, medium drop probability
        return DSCP::DSCP_AF42; 
    } else if (kind == PacketKind::TEXT) {
        // AF11: Assured Forwarding class 1, low drop probability
        return DSCP::DSCP_AF11;
    } else {
        return DSCP::DSCP_DF;
    }
}
    
} // namespace

PacketOptions::PacketOptions(PacketKind kind) 
    : PacketOptions(kind, DscpForKind(kind)) {}
    
PacketOptions::PacketOptions(PacketKind kind, DSCP dscp) 
    : kind(kind), 
      dscp(dscp) {}

PacketOptions::~PacketOptions() = default;

} // namespace naivertc
