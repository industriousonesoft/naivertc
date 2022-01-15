#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_RRTR_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_RRTR_H_

#include "base/defines.hpp"
#include "rtc/base/time/ntp_time.hpp"

namespace naivertc {
namespace rtcp {

// Receiver Reference Time Report block
class RTC_CPP_EXPORT Rrtr {
public:
    static const uint8_t kBlockType = 4;
public:
    Rrtr();
    ~Rrtr();

    size_t BlockSize() const;

    bool Parse(const uint8_t* buffer, size_t size);
    void PackInto(uint8_t* buffer, size_t size) const;

    NtpTime ntp() const;
    void set_ntp(NtpTime ntp);

private:
    NtpTime ntp_;
};
    
} // namespace rtcp    
} // namespace naivertc


#endif