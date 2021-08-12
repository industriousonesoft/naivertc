#ifndef _RTC_RTP_RTCP_RTP_RTCP_INTERFACE_H_
#define _RTC_RTP_RTCP_RTP_RTCP_INTERFACE_H_

#include "base/defines.hpp"
#include "rtc/base/clock.hpp"

#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT RtpRtcpInterface {
public:
    struct Configuration {
        // The clock to use to read time. If nullptr then system clock will be used.
        Clock* clock = nullptr;

        int rtcp_report_interval_ms = 0;

        // SSRCs for media and retransmission(RTX), respectively.
        // FlexFec SSRC is fetched from |flexfec_sender|.
        uint32_t local_media_ssrc = 0;
        std::optional<uint32_t> rtx_send_ssrc;

    };
};
    
} // namespace naivertc


#endif