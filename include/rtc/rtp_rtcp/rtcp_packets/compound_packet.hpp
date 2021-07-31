#ifndef _RTC_RTCP_COMPOUND_PACKET_H_
#define _RTC_RTCP_COMPOUND_PACKET_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp_packet.hpp"

#include <vector>

namespace naivertc {
namespace rtcp {

class RTC_CPP_EXPORT CompoundPacket : public RtcpPacket {
public:
    CompoundPacket();
    ~CompoundPacket() override;

    size_t BlockLength() const override;

    bool Create(uint8_t* packet,
                size_t* index,
                size_t max_length,
                PacketReadyCallback callback) const override;

protected:
    std::vector<std::unique_ptr<RtcpPacket>> appended_packets_;

private:
    DISALLOW_COPY_AND_ASSIGN(CompoundPacket);
};
    
} // namespace rtcp
} // namespace naivertc


#endif