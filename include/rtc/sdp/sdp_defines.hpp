#ifndef _RTC_SDP_DEFINES_H_
#define _RTC_SDP_DEFINES_H_

#include <string>
#include <unordered_map>

namespace naivertc {
namespace sdp {

enum class Type {
    UNSPEC,
    OFFER,
    ANSWER,
    PRANSWER, // provisional answer
    ROLLBACK
};

enum class Role {
    ACT_PASS,
    PASSIVE,
    ACTIVE
};

enum class Direction {
    INACTIVE,
    SEND_ONLY,
    RECV_ONLY,
    SEND_RECV
};

} // namespace sdp
} // namespace naivertc

#endif