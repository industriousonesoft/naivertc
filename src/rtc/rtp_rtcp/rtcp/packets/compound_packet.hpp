#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_COMPOUND_PACKET_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_COMPOUND_PACKET_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"

#include <vector>

namespace naivertc {
namespace rtcp {

class CompoundPacket : public RtcpPacket {
public:
    CompoundPacket();
    ~CompoundPacket() override;

    void Append(std::unique_ptr<RtcpPacket> packet);

    size_t PacketSize() const override;

    bool PackInto(uint8_t* packet,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

protected:
    std::vector<std::unique_ptr<RtcpPacket>> appended_packets_;

private:
    DISALLOW_COPY_AND_ASSIGN(CompoundPacket);
};
    
} // namespace rtcp
} // namespace naivertc


#endif