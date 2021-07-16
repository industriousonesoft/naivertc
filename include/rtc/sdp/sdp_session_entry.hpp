#ifndef _RTC_SDP_SESSION_ENTRY_H_
#define _RTC_SDP_SESSION_ENTRY_H_

#include "rtc/sdp/sdp_entry.hpp"

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT SessionEntry : public Entry {
public:
    virtual ~SessionEntry() = default;

};

} // namespace sdp
} // namespace naivert 

#endif