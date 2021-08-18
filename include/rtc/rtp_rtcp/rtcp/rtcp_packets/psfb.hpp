#ifndef _RTC_RTCP_PAYLOAD_SPECIFIC_FEEDBACK_MESSAGE_H_
#define _RTC_RTCP_PAYLOAD_SPECIFIC_FEEDBACK_MESSAGE_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"

namespace naivertc {
namespace rtcp {

// PSFB: Payload-specific feedback message are identified by  
// the value PT=PSFB as RTCP message type
// RFC 4585, Section 6.3
// Three payload-specific FB messages are defined so far plus an
//    application layer FB message.  They are identified by means of the
//    FMT parameter as follows:
//       0:     unassigned
//       1:     Picture Loss Indication (PLI)
//       2:     Slice Loss Indication (SLI)
//       3:     Reference Picture Selection Indication (RPSI)
//       4-14:  unassigned
//       15:    Application layer FB (AFB) message
//       16-30: unassigned
//       31:    reserved for future expansion of the sequence number space
//

class RTC_CPP_EXPORT Psfb : public RtcpPacket {
public:
    static constexpr uint8_t kPacketType = 206;
    // Application layer FB (AFB) message
    static constexpr uint8_t kAfbMessageType = 15;
public:
    Psfb() = default;
    ~Psfb() override = default;

    uint32_t media_ssrc() const { return media_ssrc_; }
    void set_media_ssrc(uint32_t ssrc) { media_ssrc_ = ssrc; }

protected:
    static constexpr size_t kCommonFeedbackSize = 8;
    void ParseCommonFeedback(const uint8_t* payload);
    void PackCommonFeedback(uint8_t* payload) const;

private:
    uint32_t media_ssrc_ = 0;
};
    
} // namespace rtcp
} // namespace naivert 

#endif