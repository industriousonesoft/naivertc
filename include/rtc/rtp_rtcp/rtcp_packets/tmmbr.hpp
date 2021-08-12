#ifndef _RTC_RTP_RTCP_RTCP_PACKERTS_H_
#define _RTC_RTP_RTCP_RTCP_PACKERTS_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/rtp_feedback.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/tmmb_item.hpp"

namespace naivertc {
namespace rtcp {
class CommonHeader;

// Temporary Maximum Media Stream Bit Rate Request (TMMBR).
// RFC 5104, Section 4.2.1.
class Tmmbr : public RtpFeedback {
 public:
    static constexpr uint8_t kFeedbackMessageType = 3;

    Tmmbr();
    ~Tmmbr() override;

    // Parse assumes header is already parsed and validated.
    bool Parse(const CommonHeader& packet);

    void AddTmmbr(const TmmbItem& item);

    const std::vector<TmmbItem>& requests() const { return items_; }

    size_t PacketSize() const override;

    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

 private:
    // Media ssrc is unused, shadow base class setter.
    void SetMediaSsrc(uint32_t ssrc);
    uint32_t media_ssrc() const;

    std::vector<TmmbItem> items_;
};
    
} // namespace rtcp
} // namespace naivertc


#endif