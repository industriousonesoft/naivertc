#ifndef _RTC_TRANSPORT_RTC_DATA_TRANSPORT_H_
#define _RTC_TRANSPORT_RTC_DATA_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/sctp_message.hpp"

#include <exception>

namespace naivertc {

// RtcDataTransport
class RtcDataTransport {
public:
    virtual ~RtcDataTransport() = default;
    virtual bool Send(SctpMessageToSend message) = 0;
};
    
} // namespace naivertc

#endif
