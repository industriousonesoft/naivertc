#include "rtc/rtp_rtcp/rtcp_responser.hpp"

#include <plog/Log.h>

namespace naivertc {

void RtcpResponser::SendNack(std::vector<uint16_t> nack_list,
                          bool buffering_allowed) {
    assert(buffering_allowed == true);
    rtcp_sender_.SendRtcp(RtcpPacketType::NACK, std::move(nack_list));
}

void RtcpResponser::RequestKeyFrame() {
    rtcp_sender_.SendRtcp(RtcpPacketType::PLI);
}

} // namespace naivertc