#ifndef _RTC_RTP_RTCP_RTP_FEC_FEC_RECEIVER_ULP_H_
#define _RTC_RTP_RTCP_RTP_FEC_FEC_RECEIVER_ULP_H_

#include "base/defines.hpp"

namespace naivertc {

class RTC_CPP_EXPORT UlpFecReceiver {
public:
    explicit UlpFecReceiver(uint32_t ssrc);
    ~UlpFecReceiver();

private:
    const uint32_t ssrc_;
    
};
    
} // namespace naivertc


#endif