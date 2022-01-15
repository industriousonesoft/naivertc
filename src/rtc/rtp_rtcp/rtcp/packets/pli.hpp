#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_PLI_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_PLI_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/psfb.hpp"

namespace naivertc {
namespace rtcp {
class CommonHeader;

// Picture loss indication (PLI) (RFC 4585)
class RTC_CPP_EXPORT Pli : public Psfb {
public:
    static constexpr uint8_t kFeedbackMessageType = 1;
public:
    Pli();
    Pli(const Pli&);
    ~Pli() override;

    bool Parse(const CommonHeader& packet);

    size_t PacketSize() const override;

    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

};
    
} // namespace rtcp
} // namespace naivertc

#endif