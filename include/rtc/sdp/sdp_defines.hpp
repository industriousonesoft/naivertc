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
    SEND_ONLY,
    RECV_ONLY,
    SEND_RECV,
    INACTIVE,
    UNKNOWN
};

using IceSettingPair = std::pair<std::optional<std::string>, std::optional<std::string>>;

} // namespace sdp
} // namespace naivertc

#endif