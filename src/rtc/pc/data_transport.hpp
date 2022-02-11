#ifndef _RTC_API_DATA_TRANSPORT_H_
#define _RTC_API_DATA_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/sctp_message.hpp"

#include <exception>

namespace naivertc {

// DataTransport
class RTC_CPP_EXPORT DataTransport {
public:
    virtual bool Send(SctpMessageToSend message) = 0;
protected:
    virtual ~DataTransport() = default;
};
    
} // namespace naivertc

#endif
