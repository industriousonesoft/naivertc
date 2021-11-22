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
                      Timestamp arrival_time = Timestamp::PlusInfinity());
    RtpPacketReceived(const RtpPacketReceived& other);
    RtpPacketReceived(RtpPacketReceived&& other);

    RtpPacketReceived& operator=(const RtpPacketReceived& other);
    RtpPacketReceived& operator=(RtpPacketReceived&& other);

    ~RtpPacketReceived();

    Timestamp arrival_time() const { return arrival_time_; }
    void set_arrival_time(Timestamp time) { arrival_time_ = time; }

    // Flag if packet was recovered via RTX or FEX
    bool is_recovered() const { return is_recovered_; }
    void set_is_recovered(bool recovered) { is_recovered_ = recovered; }

    int payload_type_frequency() const { return payload_type_frequency_; }
    void set_payload_type_frequency(int frequency) { payload_type_frequency_ = frequency; }

private:
    Timestamp arrival_time_ = Timestamp::PlusInfinity();
    bool is_recovered_ = false;
    int payload_type_frequency_;
};
    
} // namespace naivertc


#endif