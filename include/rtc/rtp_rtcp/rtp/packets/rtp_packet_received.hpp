#ifndef _RTC_RTP_RTCP_PACKERTS_RTP_PACKERT_RECEIVED_H_
#define _RTC_RTP_RTCP_PACKERTS_RTP_PACKERT_RECEIVED_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet.hpp"
#include "rtc/base/units/timestamp.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketReceived : public RtpPacket {
public:
    RtpPacketReceived();
    RtpPacketReceived(std::shared_ptr<ExtensionManager> extension_manager, 
                      Timestamp arrival_time = Timestamp::MaxValue());
    RtpPacketReceived(const RtpPacketReceived& other);
    RtpPacketReceived(RtpPacketReceived&& other);

    RtpPacketReceived& operator=(const RtpPacketReceived& other);
    RtpPacketReceived& operator=(RtpPacketReceived&& other);

    ~RtpPacketReceived();

    Timestamp arrival_time() const { return arrival_time_; }
    void set_arrival_time(Timestamp time) { arrival_time_ = time; }

private:
    Timestamp arrival_time_ = Timestamp::MaxValue();
};
    
} // namespace naivertc


#endif