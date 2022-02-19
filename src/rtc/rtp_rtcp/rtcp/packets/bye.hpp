#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_BYE_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_BYE_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"

#include <string>
#include <vector>

namespace naivertc {
namespace rtcp {
class CommonHeader;

class Bye : public RtcpPacket {
public:
    static constexpr uint8_t kPacketType = 203;
public:
    Bye();
    ~Bye() override;

    // Parse assumes header is already parsed and validated.
    bool Parse(const CommonHeader& packet);

    bool set_csrcs(std::vector<uint32_t> csrcs);
    void set_reason(std::string reason);

    const std::vector<uint32_t>& csrcs() const { return csrcs_; }
    const std::string& reason() const { return reason_; }

    size_t PacketSize() const override;

    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

private:
    static const int kMaxNumberOfCsrcs = 0x1f - 1;  // First item is sender SSRC.

    std::vector<uint32_t> csrcs_;
    std::string reason_;
};
    
} // namespace rtcp
} // namespace naivertc


#endif