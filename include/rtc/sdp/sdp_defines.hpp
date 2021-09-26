#ifndef _RTC_SDP_DEFINES_H_
#define _RTC_SDP_DEFINES_H_

#include "base/defines.hpp"

#include <string>
#include <unordered_map>
#include <iostream>

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

// Overload operator <<
RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, Type type);
RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, Role role);
RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, Direction direction);

} // namespace sdp
} // namespace naivertc

#endif